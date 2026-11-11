#include "othpatch/main.h"

#include "othpatch/fetch.h"
#include "othpatch/language.h"
#include "othpatch/repo.h"
#include "othpatch/update.h"

#include "core/base/arena.h"
#include "core/base/string.h"
#include "core/io/file.h"
#include "core/io/locale.h"
#include "core/io/path.h"
#include "core/json/get.h"

#include "version.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define M_DEFAULT_REPO "https://srv.thpatch.net/repo.js"
#define M_FIRST_REPO "thpatch"
#define M_LANGUAGE_PREFIX "lang_"
#define M_FALLBACK_LANGUAGE "lang_en"

// Sets the size of the arena that holds the repository network. repo.c
// limits the size of each repo.js and patch.js, so that a network of
// hundreds of repositories fits.
#define M_ARENA_SIZE ((size_t)256 * 1024 * 1024)

// Lists the games whose files othpatch can download, besides the files
// that every game shares.
static const char *const m_Games[] = { "th06" };

#define M_GAME_COUNT ((int32_t)ARRAY_SIZE(m_Games))

typedef struct {
    const char *dir;
    const char *repo;
    const char *name;
    bool list;
    bool help;
    bool version;
    bool named_games[M_GAME_COUNT];
} M_ARGS;

static const char m_Usage[] =
    "Usage: othpatch [--dir <folder>] [--repo <url>] [--game <game>]...\n"
    "                [<language> | <repo>/<patch> | <patch>]\n"
    "       othpatch --list | --help | --version\n"
    "\n"
    "Downloads thcrap patches for the opentouhou games into the patches\n"
    "folder, and updates the patches that are there already. It downloads\n"
    "the files of each game that is beside othpatch, such as oth06 or\n"
    "oth06.exe, and of each game that a patch in the folder has files for.\n"
    "\n"
    "  <language>      a language code, such as en or pt-br, for its\n"
    "                  language pack; without a name, the language of the\n"
    "                  system\n"
    "  <repo>/<patch>  a patch of one repository, such as nmlgc/aero\n"
    "  <patch>         a patch of thpatch, or else of the first repository\n"
    "                  that has it\n"
    "  --dir <folder>  the patches folder\n"
    "  --repo <url>    the first repository, " M_DEFAULT_REPO
    "\n"
    "                  by default\n"
    "  --game <game>   also download the files of a game, such as th06\n"
    "  --list          list the language packs\n"
    "  --help          print this help\n"
    "  --version       print the version\n";

PRINTF_FORMAT(2, 3)
static int M_Fail(FILE *const out, const char *const fmt, ...)
{
    fputs("othpatch: ", out);
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
    fputc('\n', out);
    return 1;
}

static int M_FailResult(
    FILE *const out, const char *const context, const RESULT result)
{
    M_Fail(out, "%s: %s", context, result.message);
    Result_Ignore(result);
    return 1;
}

static int32_t M_FindGame(const char *const id)
{
    for (int32_t i = 0; i < M_GAME_COUNT; i++) {
        if (strcmp(m_Games[i], id) == 0) {
            return i;
        }
    }
    return -1;
}

static bool M_ParseArgs(
    const int argc, char **const argv, FILE *const out, M_ARGS *const args)
{
    *args = (M_ARGS) { .repo = M_DEFAULT_REPO };
    for (int i = 1; i < argc; i++) {
        const char *const arg = argv[i];
        const bool valued = strcmp(arg, "--dir") == 0
            || strcmp(arg, "--repo") == 0 || strcmp(arg, "--game") == 0;
        if (valued && i + 1 >= argc) {
            M_Fail(out, "%s needs a value", arg);
            return false;
        }
        if (strcmp(arg, "--game") == 0) {
            const char *const id = argv[++i];
            const int32_t game = M_FindGame(id);
            if (game < 0) {
                char known[M_GAME_COUNT * 16] = "";
                size_t length = 0;
                for (int32_t k = 0; k < M_GAME_COUNT; k++) {
                    length += (size_t)snprintf(
                        known + length, sizeof(known) - length, "%s%s",
                        k > 0 ? ", " : "", m_Games[k]);
                }
                M_Fail(out, "unknown game %s; othpatch knows %s", id, known);
                return false;
            }
            args->named_games[game] = true;
        } else if (valued) {
            *(strcmp(arg, "--dir") == 0 ? &args->dir : &args->repo) = argv[++i];
        } else if (strcmp(arg, "--list") == 0) {
            args->list = true;
        } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            args->help = true;
        } else if (strcmp(arg, "--version") == 0) {
            args->version = true;
        } else if (arg[0] == '-') {
            M_Fail(out, "unknown option %s; --help lists the options", arg);
            return false;
        } else if (args->name != nullptr) {
            M_Fail(out, "name one patch only, not %s and %s", args->name, arg);
            return false;
        } else {
            args->name = arg;
        }
    }
    return true;
}

// Returns the patches folder: --dir, or <exe folder>/patches/ when a
// portable.txt is beside othpatch, as for the games, or else the folder
// of the platform. Mem_Free releases *out.
static RESULT M_FindDir(
    const M_ARGS *const args, const char *const exe_dir, char **const out)
{
    *out = nullptr;
    if (args->dir != nullptr) {
        MUST(File_MakeDir(args->dir));
        *out = Str_Dup(args->dir);
        return OK;
    }
    const char *portable_dir = nullptr;
    if (exe_dir != nullptr) {
        char *const marker =
            Path_Join(&g_HeapAllocator, exe_dir, "portable.txt");
        if (File_Exists(marker)) {
            portable_dir = exe_dir;
        }
        Mem_Free(marker);
    }
    return Path_GetPatchesDir(portable_dir, out);
}

static int M_CompareMembers(const void *const a, const void *const b)
{
    return strcmp(
        (*(const JSON_MEMBER *const *)a)->key,
        (*(const JSON_MEMBER *const *)b)->key);
}

static void M_PrintList(
    const REPO *const repo, ARENA *const arena, FILE *const out)
{
    const JSON_VALUE *const patches = repo->patches;
    const int32_t count = patches != nullptr ? patches->object.count : 0;
    const JSON_MEMBER **const packs =
        Arena_AllocArr(arena, (size_t)count + 1, sizeof(*packs));
    int32_t pack_count = 0;
    for (int32_t i = 0; i < count; i++) {
        const JSON_MEMBER *const member = &patches->object.members[i];
        if (Str_StartsWith(member->key, M_LANGUAGE_PREFIX)) {
            packs[pack_count++] = member;
        }
    }
    qsort(packs, (size_t)pack_count, sizeof(*packs), M_CompareMembers);
    for (int32_t i = 0; i < pack_count; i++) {
        fprintf(
            out, "%-20s %s\n", packs[i]->key,
            Json_GetString(packs[i]->value, ""));
    }
}

// Picks the language pack of the system locale that the first repository
// has, or the fallback, and writes why into reason.
static const char *M_PickLanguage(
    const REPO *const first,
    char names[LANGUAGE_MAX_CANDIDATES][LANGUAGE_NAME_SIZE], char *const reason,
    const size_t reason_size)
{
    const LOCALE locale = Locale_GetPreferred();
    char code[2 * LOCALE_CODE_SIZE];
    snprintf(
        code, sizeof(code), "%s%s%s", locale.language,
        locale.country[0] != '\0' ? "_" : "", locale.country);
    const int32_t count = Language_FromLocale(locale, names);
    for (int32_t i = 0; i < count; i++) {
        if (Repo_HasPatch(first, names[i])) {
            snprintf(reason, reason_size, "from the system locale %s", code);
            return names[i];
        }
    }
    if (count == 0) {
        snprintf(
            reason, reason_size,
            "the system locale is unknown, so English is the default");
    } else {
        snprintf(
            reason, reason_size,
            "the system locale %s has no language pack, so English is the "
            "default",
            code);
    }
    return M_FALLBACK_LANGUAGE;
}

static bool M_Contains(const REPO_PATCH_LIST *const list, const char *const id)
{
    for (int32_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i]->id, id) == 0) {
            return true;
        }
    }
    return false;
}

static bool M_IsLanguagePack(const char *const id)
{
    return Str_StartsWith(id, M_FIRST_REPO "/" M_LANGUAGE_PREFIX);
}

static bool M_IsBeside(
    ARENA *const arena, const char *const exe_dir, const char *const game)
{
    static const char *const suffixes[] = { "", ".exe", ".AppImage", ".app" };
    for (size_t i = 0; exe_dir != nullptr && i < ARRAY_SIZE(suffixes); i++) {
        char name[64];
        snprintf(name, sizeof(name), "o%s%s", game, suffixes[i]);
        if (File_Exists(Path_Join(Arena_Allocator(arena), exe_dir, name))) {
            return true;
        }
    }
    return false;
}

static bool M_IsDownloaded(
    ARENA *const arena, const char *const dir,
    const UPDATE_INSTALLED_LIST *const installed, const char *const game)
{
    const ALLOCATOR *const allocator = Arena_Allocator(arena);
    for (int32_t i = 0; i < installed->count; i++) {
        const char *const patch_dir =
            Path_Join(allocator, dir, installed->items[i].id);
        if (File_Exists(Path_Join(allocator, patch_dir, game))) {
            return true;
        }
    }
    return false;
}

// Picks the games whose files to download: the games named with --game,
// the games beside othpatch and the games that an installed patch has a
// folder for. Prints them with the reason for each, and returns how many
// there are.
static int32_t M_PickGames(
    const M_ARGS *const args, const char *const exe_dir, const char *const dir,
    const UPDATE_INSTALLED_LIST *const installed, ARENA *const arena,
    const char *games[M_GAME_COUNT], FILE *const out)
{
    int32_t count = 0;
    for (int32_t i = 0; i < M_GAME_COUNT; i++) {
        const char *reason = nullptr;
        if (args->named_games[i]) {
            reason = "named on the command line";
        } else if (M_IsBeside(arena, exe_dir, m_Games[i])) {
            reason = "beside othpatch";
        } else if (M_IsDownloaded(arena, dir, installed, m_Games[i])) {
            reason = "in the patches folder";
        }
        if (reason != nullptr) {
            fprintf(
                out, "%s%s (%s)", count == 0 ? "Games: " : ", ", m_Games[i],
                reason);
            games[count++] = m_Games[i];
        }
    }
    if (count > 0) {
        fputc('\n', out);
    }
    return count;
}

static int M_Run(
    const M_ARGS *const args, const char *const exe_dir, const char *const dir,
    ARENA *const arena, FILE *const out)
{
    UPDATE_INSTALLED_LIST installed;
    const char *games[M_GAME_COUNT];
    int32_t game_count = 0;
    if (!args->list) {
        const RESULT listed = Update_ListInstalled(dir, arena, &installed);
        if (!IS_OK(listed)) {
            return M_FailResult(out, "cannot list the patches folder", listed);
        }
        game_count =
            M_PickGames(args, exe_dir, dir, &installed, arena, games, out);
        if (game_count == 0) {
            return M_Fail(
                out,
                "no opentouhou game is beside othpatch; name one with "
                "--game %s",
                m_Games[0]);
        }
    }

    REPO_NETWORK *network;
    const RESULT opened = Repo_Open(args->repo, arena, &network);
    if (!IS_OK(opened)) {
        return M_FailResult(out, "cannot read the repository", opened);
    }
    const REPO *const first = Repo_GetFirst(network);
    if (args->list) {
        M_PrintList(first, arena, out);
        return 0;
    }

    char names[LANGUAGE_MAX_CANDIDATES][LANGUAGE_NAME_SIZE];
    char reason[160] = "named on the command line";
    const char *const name = args->name != nullptr
        ? Language_FromArgument(args->name, names[0])
        : M_PickLanguage(first, names, reason, sizeof(reason));

    REPO_PATCH_LIST list;
    Array_Init(&list, Arena_Allocator(arena));
    const RESULT added = Repo_AddPatch(network, name, &list);
    if (!IS_OK(added)) {
        return M_FailResult(out, name, added);
    }
    const REPO_PATCH *const target = list.items[list.count - 1];
    const bool language = M_IsLanguagePack(target->id);
    fprintf(
        out, "%s: %s (%s), %s\n", language ? "Language" : "Patch", target->id,
        target->title, reason);

    typedef ARRAY(const char *) M_IDS;
    M_IDS removals;
    Array_Init(&removals, Arena_Allocator(arena));
    for (int32_t i = 0; i < installed.count; i++) {
        const UPDATE_INSTALLED *const patch = &installed.items[i];
        if (M_Contains(&list, patch->id)) {
            continue;
        }
        if (language && M_IsLanguagePack(patch->id)) {
            Array_Append(&removals, patch->id);
            continue;
        }
        if (!patch->update) {
            continue;
        }
        const RESULT result = Repo_AddPatch(network, patch->id, &list);
        if (!IS_OK(result)) {
            fprintf(
                out, "%s: left as it is, cannot update it: %s\n", patch->id,
                result.message);
            Result_Ignore(result);
        }
    }

    UPDATE_COUNTS total = {};
    int32_t failed = 0;
    bool target_ok = false;
    for (int32_t i = 0; i < list.count; i++) {
        const REPO_PATCH *const patch = list.items[i];
        UPDATE_COUNTS counts;
        const RESULT result =
            Update_Patch(dir, patch, games, game_count, &counts);
        if (!IS_OK(result)) {
            fprintf(out, "%s: failed: %s\n", patch->id, result.message);
            Result_Ignore(result);
            failed++;
            continue;
        }
        target_ok = target_ok || patch == target;
        fprintf(
            out, "%s: %d downloaded, %d up to date, %d removed\n", patch->id,
            counts.downloaded, counts.up_to_date, counts.removed);
        total.downloaded += counts.downloaded;
        total.up_to_date += counts.up_to_date;
        total.removed += counts.removed;
    }

    for (int32_t i = 0; target_ok && i < removals.count; i++) {
        const RESULT result = Update_RemovePatch(dir, removals.items[i]);
        if (IS_OK(result)) {
            fprintf(
                out, "%s: removed, %s replaces it\n", removals.items[i],
                target->id);
        } else {
            fprintf(
                out, "%s: cannot remove it: %s\n", removals.items[i],
                result.message);
            Result_Ignore(result);
            failed++;
        }
    }

    if (failed > 0) {
        return M_Fail(
            out, "%d %s failed; run othpatch again to retry", failed,
            failed == 1 ? "patch" : "patches");
    }
    fprintf(
        out,
        "Done: %d patches, %d files downloaded, %d up to date, %d "
        "removed\n",
        list.count, total.downloaded, total.up_to_date, total.removed);
    return 0;
}

int Main_Run(
    const int argc, char **const argv, const char *const exe_dir,
    FILE *const out)
{
    M_ARGS args;
    if (!M_ParseArgs(argc, argv, out, &args)) {
        return 1;
    }
    if (args.help) {
        fputs(m_Usage, out);
        return 0;
    }
    if (args.version) {
        fprintf(out, "othpatch %s\n", OTHPATCH_VERSION);
        return 0;
    }
    char *dir = nullptr;
    if (!args.list) {
        const RESULT found = M_FindDir(&args, exe_dir, &dir);
        if (!IS_OK(found)) {
            return M_FailResult(out, "cannot find the patches folder", found);
        }
        fprintf(out, "Patches folder: %s\n", dir);
    }
    int code = 1;
    const RESULT started = Fetch_Init();
    if (IS_OK(started)) {
        ARENA arena;
        Arena_Init(&arena, "othpatch", M_ARENA_SIZE);
        code = M_Run(&args, exe_dir, dir, &arena, out);
        Arena_Free(&arena);
    } else {
        M_FailResult(out, "cannot start", started);
    }
    Fetch_Shutdown();
    Mem_Free(dir);
    fflush(out);
    return code;
}
