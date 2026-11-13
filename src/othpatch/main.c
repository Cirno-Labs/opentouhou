#include "othpatch/main.h"

#include "othpatch/fetch.h"
#include "othpatch/game.h"
#include "othpatch/install.h"
#include "othpatch/release.h"
#include "othpatch/repo.h"
#include "othpatch/update.h"

#include "shell/folders.h"
#include "core/base/arena.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/hash/sha256.h"
#include "core/ini.h"
#include "core/io/file.h"
#include "core/io/locale.h"
#include "core/io/path.h"
#include "core/json/get.h"
#include "core/json/parse.h"
#include "core/patch/language.h"
#include "core/patch/select.h"

#include "version.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define M_DEFAULT_REPO "https://srv.thpatch.net/repo.js"
#define M_LANGUAGE_PREFIX "lang_"
#define M_FALLBACK_LANGUAGE "lang_en"
#define M_ORIGINAL_TITLE "original Japanese"
#define M_MAX_AMBIGUOUS 8
#define M_BAR_CELLS 20
#define M_MEGABYTE (1024.0 * 1024.0)

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
    const char *releases;
    const char *name;
    const char *remove;
    bool list;
    bool help;
    bool version;
    bool update;
    bool verbose;
} M_ARGS;

// Holds what every step of one run uses.
typedef struct {
    const M_ARGS *args;
    const char *exe_dir;
    const char *dir;
    ARENA *arena;
    FILE *in;
    FILE *out;
    // Tells whether out is a terminal, where a progress bar redraws in
    // place.
    bool terminal;
    int32_t sections;
} M_RUN;

// Holds a progress bar: a label padded to width code points, and the
// files or the bytes that have arrived.
typedef struct {
    M_RUN *run;
    const char *label;
    int32_t width;
    int32_t done;
    int32_t total;
    size_t received;
    size_t expected;
    // Holds the line last drawn, so that a report that changes nothing
    // draws nothing.
    char line[512];
} M_BAR;

typedef enum {
    M_KIND_SETUP,
    M_KIND_UPDATE,
    M_KIND_INSTALL,
} M_KIND;

// Holds the state of the language section.
typedef struct {
    M_RUN *run;
    UPDATE_INSTALLED_LIST installed;
    const char *games[M_GAME_COUNT];
    int32_t game_count;
    M_KIND kind;
    // Holds the patch that the heading of M_KIND_INSTALL names.
    const char *target;
    bool headed;
} M_LANGUAGE;

// Holds the state of the update checks of one run.
typedef struct {
    M_RUN *run;
    const INSTALL_PLATFORM *platform;
    // Holds the version of the othpatch in exe_dir: the running one, or
    // the one that an update installed. othpatch_is_release is false for a
    // development build, which is never replaced.
    RELEASE_VERSION othpatch_version;
    bool othpatch_is_release;
    // Holds whether the release index was fetched, and the index, or
    // nullptr with the reason when the fetch failed.
    bool releases_fetched;
    JSON_VALUE *releases;
    const char *releases_error;
} M_UPDATE;

static const char m_Usage[] =
    "Usage: othpatch [--update] [--verbose]\n"
    "                [<language> | <repo>/<patch> | <patch>]\n"
    "       othpatch --remove <patch>\n"
    "       othpatch --list | --help | --version\n"
    "\n"
    "Downloads thcrap patches for the opentouhou games beside othpatch,\n"
    "such as oth06 or oth06.exe, and updates the patches that you have.\n"
    "Then it checks for a newer release of othpatch and of each game, and\n"
    "asks before it installs one.\n"
    "\n"
    "Without an argument, othpatch updates the language and every patch\n"
    "that you have. When no language is set yet, it sets the language of\n"
    "the system.\n"
    "\n"
    "  <language>        a language code, such as de or pt-br, or ja for\n"
    "                    the original Japanese\n"
    "  <repo>/<patch>    a patch of one repository, such as nmlgc/aero\n"
    "  <patch>           the patch of the one repository that has it\n"
    "  --remove <patch>  remove a patch and the patches that only it needs\n"
    "  --list            list the languages and the installed mods\n"
    "  --update          install new releases without asking\n"
    "  --verbose         also print the folders and each patch\n"
    "  -h, --help        print this help\n"
    "  -v, --version     print the version\n"
    "\n"
    "Developer options:\n"
    "  --dir <folder>    the patches folder\n"
    "  --repo <url>      the first repository, " M_DEFAULT_REPO
    "\n"
    "                    by default\n"
    "  --releases <url>  the release index of the games and othpatch,\n"
    "                    " OTHPATCH_RELEASES_URL " by default\n";

static const char m_Unchanged[] =
    "Your files are unchanged. Run othpatch again later.\n";

PRINTF_FORMAT(2, 3)
static char *M_Format(ARENA *const arena, const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *const heap = Str_FormatV(fmt, args);
    va_end(args);
    const size_t size = strlen(heap) + 1;
    char *const copy = Arena_Alloc(arena, size);
    memcpy(copy, heap, size);
    Mem_Free(heap);
    return copy;
}

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

// Prints "<what>: <reason>." and consumes result.
static void M_PrintFailure(
    FILE *const out, const char *const what, const RESULT result)
{
    fprintf(out, "%s: %s.\n", what, result.message);
    Result_Ignore(result);
}

static bool M_ParseArgs(
    const int argc, char **const argv, FILE *const out, M_ARGS *const args)
{
    *args = (M_ARGS) {
        .repo = M_DEFAULT_REPO,
        .releases = OTHPATCH_RELEASES_URL,
    };
    for (int i = 1; i < argc; i++) {
        const char *const arg = argv[i];
        const bool valued = strcmp(arg, "--dir") == 0
            || strcmp(arg, "--repo") == 0 || strcmp(arg, "--releases") == 0
            || strcmp(arg, "--remove") == 0;
        if (valued && i + 1 >= argc) {
            M_Fail(out, "%s needs a value", arg);
            return false;
        }
        if (strcmp(arg, "--dir") == 0) {
            args->dir = argv[++i];
        } else if (strcmp(arg, "--repo") == 0) {
            args->repo = argv[++i];
        } else if (strcmp(arg, "--releases") == 0) {
            args->releases = argv[++i];
        } else if (strcmp(arg, "--remove") == 0) {
            args->remove = argv[++i];
        } else if (strcmp(arg, "--update") == 0) {
            args->update = true;
        } else if (strcmp(arg, "--verbose") == 0) {
            args->verbose = true;
        } else if (strcmp(arg, "--list") == 0) {
            args->list = true;
        } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            args->help = true;
        } else if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
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
    if (args->list && args->remove != nullptr) {
        M_Fail(out, "use --list or --remove, not both");
        return false;
    }
    if (args->name != nullptr && (args->remove != nullptr || args->list)) {
        M_Fail(
            out, "%s takes no patch to install",
            args->remove != nullptr ? "--remove" : "--list");
        return false;
    }
    return true;
}

// Returns the patches folder: --dir, or else the patches folder of the
// games beside othpatch. Mem_Free releases *out.
static RESULT M_FindDir(
    const M_ARGS *const args, const char *const exe_dir, char **const out)
{
    *out = nullptr;
    if (args->dir != nullptr) {
        MUST(File_MakeDir(args->dir));
        *out = Str_Dup(args->dir);
        return OK;
    }
    const FOLDERS_INPUT input = { .exe_dir = exe_dir };
    return Folders_FindPatchesDir(&input, out);
}

// Starts a section: a blank line after the section before, then heading.
PRINTF_FORMAT(2, 3)
static void M_StartSection(M_RUN *const run, const char *const fmt, ...)
{
    if (run->sections++ > 0) {
        fputc('\n', run->out);
    }
    va_list args;
    va_start(args, fmt);
    vfprintf(run->out, fmt, args);
    va_end(args);
    fputc('\n', run->out);
}

static int32_t M_CountCodePoints(const char *const text)
{
    int32_t count = 0;
    for (const char *c = text; *c != '\0'; c++) {
        count += ((unsigned char)*c & 0xC0) != 0x80;
    }
    return count;
}

// Draws the bar on a terminal, filled to fraction, with amount after it.
static void M_DrawBar(
    M_BAR *const bar, const double fraction, const char *const amount)
{
    if (!bar->run->terminal) {
        return;
    }
    char cells[M_BAR_CELLS + 1];
    const int32_t filled =
        (int32_t)(MIN(MAX(fraction, 0.0), 1.0) * M_BAR_CELLS);
    for (int32_t i = 0; i < M_BAR_CELLS; i++) {
        cells[i] = i < filled ? '#' : '.';
    }
    cells[M_BAR_CELLS] = '\0';
    const int32_t padding = MAX(bar->width - M_CountCodePoints(bar->label), 0);
    char line[sizeof(bar->line)];
    snprintf(
        line, sizeof(line), "%s%*s [%s] %s", bar->label, (int)padding, "",
        cells, amount);
    if (strcmp(line, bar->line) == 0) {
        return;
    }
    snprintf(bar->line, sizeof(bar->line), "%s", line);
    fprintf(bar->run->out, "\r%s", line);
    fflush(bar->run->out);
}

// Ends the bar: on a terminal, draws it a last time and ends its line;
// elsewhere, prints the label and plain.
static void M_EndBar(
    M_BAR *const bar, const double fraction, const char *const amount,
    const char *const plain)
{
    if (bar->run->terminal) {
        M_DrawBar(bar, fraction, amount);
        fputc('\n', bar->run->out);
    } else {
        fprintf(bar->run->out, "%s: %s\n", bar->label, plain);
    }
}

static const char *M_Files(const int32_t total)
{
    return total == 1 ? "file" : "files";
}

static void M_ReportFiles(
    void *const context, const int32_t done, const int32_t total)
{
    M_BAR *const bar = context;
    bar->done = done;
    bar->total = total;
    char amount[64];
    snprintf(amount, sizeof(amount), "%d/%d %s", done, total, M_Files(total));
    M_DrawBar(bar, (double)done / total, amount);
}

static void M_EndFiles(M_BAR *const bar)
{
    char amount[64];
    snprintf(
        amount, sizeof(amount), "%d/%d %s", bar->done, bar->total,
        M_Files(bar->total));
    char plain[64];
    if (bar->done == bar->total) {
        snprintf(plain, sizeof(plain), "%d %s", bar->done, M_Files(bar->total));
    } else {
        snprintf(plain, sizeof(plain), "%s", amount);
    }
    M_EndBar(bar, (double)bar->done / bar->total, amount, plain);
}

static void M_FormatBytes(
    const M_BAR *const bar, char amount[64], double *const fraction)
{
    if (bar->expected == 0) {
        snprintf(amount, 64, "%.1f MB", (double)bar->received / M_MEGABYTE);
        *fraction = 0.0;
        return;
    }
    snprintf(
        amount, 64, "%.1f/%.1f MB", (double)bar->received / M_MEGABYTE,
        (double)bar->expected / M_MEGABYTE);
    *fraction = (double)bar->received / (double)bar->expected;
}

static void M_ReportBytes(
    void *const context, const size_t received, const size_t expected)
{
    M_BAR *const bar = context;
    bar->received = received;
    bar->expected = expected;
    char amount[64];
    double fraction;
    M_FormatBytes(bar, amount, &fraction);
    M_DrawBar(bar, fraction, amount);
}

static void M_EndBytes(M_BAR *const bar, const bool arrived)
{
    if (arrived) {
        bar->expected = bar->received;
    }
    char amount[64];
    double fraction;
    M_FormatBytes(bar, amount, &fraction);
    char plain[64];
    if (arrived) {
        snprintf(
            plain, sizeof(plain), "%.1f MB",
            (double)bar->received / M_MEGABYTE);
    } else {
        snprintf(plain, sizeof(plain), "%s", amount);
    }
    M_EndBar(bar, fraction, amount, plain);
}

static int M_CompareMembers(const void *const a, const void *const b)
{
    return strcmp(
        (*(const JSON_MEMBER *const *)a)->key,
        (*(const JSON_MEMBER *const *)b)->key);
}

// Returns the language packs of repo, in byte order of their names.
static int32_t M_SortPacks(
    const REPO *const repo, ARENA *const arena, const JSON_MEMBER ***const out)
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
    *out = packs;
    return pack_count;
}

static const char *M_FindLanguagePack(
    const REPO *const repo, const char *const tag)
{
    const JSON_VALUE *const patches = repo->patches;
    const int32_t count = patches != nullptr ? patches->object.count : 0;
    for (int32_t i = 0; i < count; i++) {
        const char *const key = patches->object.members[i].key;
        if (Str_StartsWith(key, M_LANGUAGE_PREFIX)
            && Patch_LanguageIs(key + strlen(M_LANGUAGE_PREFIX), tag)) {
            return key;
        }
    }
    return nullptr;
}

static const char *M_FindLanguageOfCode(
    const REPO *const repo, const char *const code)
{
    char tags[PATCH_LANGUAGE_MAX_TAGS][PATCH_LANGUAGE_TAG_SIZE];
    const int32_t count = Patch_GetLanguageTags(code, tags);
    for (int32_t i = 0; i < count; i++) {
        const char *const pack = M_FindLanguagePack(repo, tags[i]);
        if (pack != nullptr) {
            return pack;
        }
    }
    return nullptr;
}

// Returns the title of the language pack name of repo, as repo.js gives
// it, or the code of the pack when repo.js gives none.
static const char *M_GetPackTitle(
    const REPO *const repo, const char *const name)
{
    const char *const code = name + strlen(M_LANGUAGE_PREFIX);
    const char *const title = Json_GetString(Json_Get(repo->patches, name), "");
    return title[0] != '\0' ? title : code;
}

static bool M_IsOriginal(const char *const name)
{
    char tags[PATCH_LANGUAGE_MAX_TAGS][PATCH_LANGUAGE_TAG_SIZE];
    return Patch_GetLanguageTags(name, tags) > 0
        && Patch_IsOriginalLanguage(tags[0]);
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

static bool M_IsInstalled(
    const UPDATE_INSTALLED_LIST *const installed, const char *const id)
{
    for (int32_t i = 0; i < installed->count; i++) {
        if (strcmp(installed->items[i].id, id) == 0) {
            return true;
        }
    }
    return false;
}

static bool M_IsBeside(
    ARENA *const arena, const char *const exe_dir, const char *const game)
{
    static const char *const suffixes[] = { "", ".exe", ".AppImage", ".app" };
    char product[GAME_NAME_SIZE];
    Game_GetProductName(game, product);
    for (size_t i = 0; exe_dir != nullptr && i < ARRAY_SIZE(suffixes); i++) {
        char name[GAME_NAME_SIZE + 16];
        snprintf(name, sizeof(name), "%s%s", product, suffixes[i]);
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

// Picks the games whose files to download: the games beside othpatch and
// the games that an installed patch has a folder for. Returns how many
// there are.
static int32_t M_PickGames(
    const char *const exe_dir, const char *const dir,
    const UPDATE_INSTALLED_LIST *const installed, ARENA *const arena,
    const char *games[M_GAME_COUNT])
{
    int32_t count = 0;
    for (int32_t i = 0; i < M_GAME_COUNT; i++) {
        if (M_IsBeside(arena, exe_dir, m_Games[i])
            || M_IsDownloaded(arena, dir, installed, m_Games[i])) {
            games[count++] = m_Games[i];
        }
    }
    return count;
}

// Returns the product names of games, separated by ", ", or of every game
// that othpatch knows when there are none. The string comes from arena.
static const char *M_ListProducts(
    const char *const *const games, const int32_t game_count,
    ARENA *const arena)
{
    const char *const *const names = game_count > 0 ? games : m_Games;
    const int32_t count = game_count > 0 ? game_count : M_GAME_COUNT;
    const char *text = "";
    for (int32_t i = 0; i < count; i++) {
        char product[GAME_NAME_SIZE];
        Game_GetProductName(names[i], product);
        text = M_Format(arena, "%s%s%s", text, i > 0 ? ", " : "", product);
    }
    return text;
}

// Returns the config.ini of game beside othpatch, started without
// --user-dir. The path comes from arena.
static RESULT M_FindConfig(
    const char *const exe_dir, const char *const game, ARENA *const arena,
    const char **const out)
{
    *out = nullptr;
    const FOLDERS_INPUT input = { .exe_dir = exe_dir };
    FOLDERS folders;
    const RESULT result = Folders_Find(&input, game, &folders);
    if (IS_OK(result)) {
        *out =
            Path_Join(Arena_Allocator(arena), folders.user_dir, "config.ini");
    }
    Folders_Free(&folders);
    return result;
}

static RESULT M_WriteLanguage(const char *const path, const char *const code)
{
    INI *ini;
    MUST(Ini_Load(path, &g_HeapAllocator, &ini));
    Ini_Set(ini, "patches", "language", code);
    const RESULT result = Ini_Save(ini, path);
    Ini_Free(ini);
    return result;
}

// Returns the [patches] language of the first of games that sets one, or
// nullptr when none does. The string comes from arena.
static const char *M_ReadLanguage(
    const char *const exe_dir, const char *const *const games,
    const int32_t game_count, ARENA *const arena)
{
    for (int32_t i = 0; i < game_count; i++) {
        const char *path;
        INI *ini = nullptr;
        if (!Result_Ignore(M_FindConfig(exe_dir, games[i], arena, &path))
            || !Result_Ignore(Ini_Load(path, &g_HeapAllocator, &ini))) {
            continue;
        }
        const char *const code = Ini_Get(ini, "patches", "language");
        const char *const copy = code != nullptr && code[0] != '\0'
            ? M_Format(arena, "%s", code)
            : nullptr;
        Ini_Free(ini);
        if (copy != nullptr) {
            return copy;
        }
    }
    return nullptr;
}

// Returns the codes of the language packs of repo, separated by ", ". The
// string comes from arena.
static const char *M_ListCodes(const REPO *const repo, ARENA *const arena)
{
    const JSON_MEMBER **packs;
    const int32_t pack_count = M_SortPacks(repo, arena, &packs);
    const char *text = "";
    for (int32_t i = 0; i < pack_count; i++) {
        text = M_Format(
            arena, "%s%s, ", text, packs[i]->key + strlen(M_LANGUAGE_PREFIX));
    }
    return text;
}

static const char *M_GetLabel(const REPO_PATCH *const patch, ARENA *const arena)
{
    if (strcmp(patch->title, patch->id) == 0) {
        return patch->id;
    }
    return M_Format(arena, "%s (%s)", patch->title, patch->id);
}

// Prints the heading of the language section on the first call, and with
// --verbose the folders that it uses.
static void M_Head(M_LANGUAGE *const language)
{
    if (language->headed) {
        return;
    }
    language->headed = true;
    M_RUN *const run = language->run;
    if (language->kind == M_KIND_INSTALL) {
        M_StartSection(run, "Installing %s...", language->target);
    } else if (language->kind == M_KIND_UPDATE) {
        M_StartSection(run, "Updating the language...");
    } else {
        M_StartSection(run, "Setting up the language...");
    }
    if (!run->args->verbose) {
        return;
    }
    fprintf(run->out, "Patches folder: %s\n", run->dir);
    for (int32_t i = 0; i < language->game_count; i++) {
        const char *path;
        if (Result_Ignore(M_FindConfig(
                run->exe_dir, language->games[i], run->arena, &path))) {
            char product[GAME_NAME_SIZE];
            Game_GetProductName(language->games[i], product);
            fprintf(run->out, "config.ini of %s: %s\n", product, path);
        }
    }
}

static void M_PrintNoLanguage(
    const M_LANGUAGE *const language, const REPO *const first,
    const char *const code)
{
    ARENA *const arena = language->run->arena;
    fprintf(
        language->run->out,
        "There is no language \"%s\".\n"
        "Languages for %s: %sja (original)\n",
        code, M_ListProducts(language->games, language->game_count, arena),
        M_ListCodes(first, arena));
}

static bool M_OpenNetwork(
    M_LANGUAGE *const language, REPO_NETWORK **const network)
{
    M_RUN *const run = language->run;
    const RESULT opened = Repo_Open(run->args->repo, run->arena, network);
    if (IS_OK(opened)) {
        return true;
    }
    M_Head(language);
    M_PrintFailure(run->out, "Could not download the patch list", opened);
    fputs(m_Unchanged, run->out);
    return false;
}

// Adds the installed patches that let themselves update to list. A patch
// that the network lacks stays as it is.
static void M_AddInstalled(
    const M_LANGUAGE *const language, REPO_NETWORK *const network,
    REPO_PATCH_LIST *const list)
{
    M_RUN *const run = language->run;
    for (int32_t i = 0; i < language->installed.count; i++) {
        const UPDATE_INSTALLED *const patch = &language->installed.items[i];
        if (!patch->update || M_Contains(list, patch->id)) {
            continue;
        }
        const RESULT result = Repo_AddPatch(network, patch->id, list);
        if (IS_OK(result)) {
            continue;
        }
        if (run->args->verbose) {
            fprintf(
                run->out, "%s: left as it is, cannot update it: %s\n",
                patch->id, result.message);
        }
        Result_Ignore(result);
    }
}

static bool M_HasUpdates(const M_LANGUAGE *const language)
{
    for (int32_t i = 0; i < language->installed.count; i++) {
        if (language->installed.items[i].update) {
            return true;
        }
    }
    return false;
}

// Brings every patch of list up to date, and sets *changed to whether a
// file was downloaded or removed. The first target_count patches are the
// patch that the section installs, last, and its dependencies; they update
// first, from the last, so that the patch comes before what it needs.
// Stops at the first patch that fails.
static bool M_UpdateList(
    const M_LANGUAGE *const language, const REPO_PATCH_LIST *const list,
    const int32_t target_count, bool *const changed)
{
    M_RUN *const run = language->run;
    *changed = false;
    int32_t width = 0;
    for (int32_t i = 0; i < list->count; i++) {
        width = MAX(
            width, M_CountCodePoints(M_GetLabel(list->items[i], run->arena)));
    }
    for (int32_t i = 0; i < list->count; i++) {
        const REPO_PATCH *const patch =
            list->items[i < target_count ? target_count - 1 - i : i];
        M_BAR bar = {
            .run = run,
            .label = M_GetLabel(patch, run->arena),
            .width = width,
        };
        const UPDATE_PROGRESS progress = {
            .report = M_ReportFiles,
            .context = &bar,
        };
        UPDATE_COUNTS counts;
        const RESULT result = Update_Patch(
            run->dir, patch, language->games, language->game_count, &progress,
            &counts);
        if (bar.total > 0) {
            M_EndFiles(&bar);
        }
        if (!IS_OK(result)) {
            const char *const what =
                M_Format(run->arena, "Could not download %s", patch->id);
            M_PrintFailure(run->out, what, result);
            fputs(m_Unchanged, run->out);
            return false;
        }
        if (run->args->verbose && counts.downloaded == 0
            && counts.removed == 0) {
            fprintf(run->out, "%s: up to date\n", patch->id);
        } else if (run->args->verbose) {
            fprintf(
                run->out, "%s: %d downloaded, %d up to date, %d removed\n",
                patch->id, counts.downloaded, counts.up_to_date,
                counts.removed);
        }
        *changed = *changed || counts.downloaded > 0 || counts.removed > 0;
    }
    return true;
}

static void M_PrintUpdated(FILE *const out, const bool changed)
{
    if (!changed) {
        fputs("Everything was already up to date.\n", out);
    }
}

// Sets [patches] language to code in the config.ini of each game.
static bool M_SaveLanguage(
    const M_LANGUAGE *const language, const char *const code)
{
    M_RUN *const run = language->run;
    bool saved = true;
    for (int32_t i = 0; i < language->game_count; i++) {
        const char *path;
        const RESULT found =
            M_FindConfig(run->exe_dir, language->games[i], run->arena, &path);
        if (!IS_OK(found)) {
            char product[GAME_NAME_SIZE];
            Game_GetProductName(language->games[i], product);
            M_PrintFailure(
                run->out,
                M_Format(
                    run->arena, "Could not find the config.ini of %s", product),
                found);
            saved = false;
            continue;
        }
        const RESULT written = M_WriteLanguage(path, code);
        if (IS_OK(written)) {
            fprintf(run->out, "Saved language = %s to %s\n", code, path);
        } else {
            M_PrintFailure(
                run->out,
                M_Format(
                    run->arena, "Could not save language = %s to %s", code,
                    path),
                written);
            saved = false;
        }
    }
    return saved;
}

static bool M_HasLanguagePack(const M_LANGUAGE *const language)
{
    for (int32_t i = 0; i < language->installed.count; i++) {
        if (Patch_IsLanguagePack(language->installed.items[i].id)) {
            return true;
        }
    }
    return false;
}

static bool M_SetOriginal(M_LANGUAGE *const language, const bool system)
{
    M_Head(language);
    FILE *const out = language->run->out;
    fprintf(
        out, "%s: " M_ORIGINAL_TITLE "\n",
        system ? "System language" : "Language");
    if (!M_SaveLanguage(language, PATCH_LANGUAGE_ORIGINAL)) {
        return false;
    }
    if (M_HasLanguagePack(language)) {
        fputs(
            "The other language packs stay installed, so switching back "
            "downloads nothing.\n",
            out);
    }
    return true;
}

static bool M_UpdateOriginal(M_LANGUAGE *const language)
{
    M_Head(language);
    M_RUN *const run = language->run;
    fputs("Language: " M_ORIGINAL_TITLE "\n", run->out);
    if (!M_HasUpdates(language)) {
        M_PrintUpdated(run->out, false);
        return true;
    }
    REPO_NETWORK *network;
    if (!M_OpenNetwork(language, &network)) {
        return false;
    }
    REPO_PATCH_LIST list;
    Array_Init(&list, Arena_Allocator(run->arena));
    M_AddInstalled(language, network, &list);
    bool changed;
    if (!M_UpdateList(language, &list, 0, &changed)) {
        return false;
    }
    M_PrintUpdated(run->out, changed);
    return true;
}

// Finds the patch that the language section installs: the language pack
// of code for a language, or the patch that the argument names. Sets
// *pack to the repository and *pack_name to the name of a language pack,
// or both to nullptr for another patch.
static bool M_Resolve(
    M_LANGUAGE *const language, REPO_NETWORK *const network,
    const char *const code, const bool system, const char **const id,
    const REPO **const pack, const char **const pack_name, bool *const matched)
{
    M_RUN *const run = language->run;
    const REPO *const first = Repo_GetFirst(network);
    *pack = nullptr;
    *pack_name = nullptr;
    *matched = true;
    if (strchr(code, '/') != nullptr) {
        *id = code;
        if (Patch_IsLanguagePack(code)) {
            const size_t length = (size_t)(strchr(code, '/') - code);
            *pack = Repo_FindRepo(
                network, M_Format(run->arena, "%.*s", (int)length, code));
            *pack_name = code + length + 1;
        }
        return true;
    }
    const char *const found = M_FindLanguageOfCode(first, code);
    if (found != nullptr || system) {
        *matched = found != nullptr;
        *pack = first;
        *pack_name = found != nullptr ? found : M_FALLBACK_LANGUAGE;
        *id = M_Format(run->arena, "%s/%s", first->id, *pack_name);
        return true;
    }
    if (language->kind == M_KIND_UPDATE) {
        M_Head(language);
        M_PrintNoLanguage(language, first, code);
        return false;
    }
    const REPO *repos[M_MAX_AMBIGUOUS];
    const int32_t count =
        Repo_FindAllPatch(network, code, repos, M_MAX_AMBIGUOUS);
    if (count == 0) {
        M_Head(language);
        M_PrintNoLanguage(language, first, code);
        return false;
    }
    language->kind = M_KIND_INSTALL;
    if (count == 1) {
        *id = M_Format(run->arena, "%s/%s", repos[0]->id, code);
        return true;
    }
    language->target = code;
    M_Head(language);
    fprintf(run->out, "%d repositories have a patch %s:", count, code);
    for (int32_t i = 0; i < MIN(count, M_MAX_AMBIGUOUS); i++) {
        fprintf(run->out, "%s %s/%s", i > 0 ? "," : "", repos[i]->id, code);
    }
    fputs(". Name one as <repo>/<patch>.\n", run->out);
    return false;
}

// Installs the language pack of code, or the patch that code names, with
// every installed patch that updates. system tells whether code is the
// language of the system.
static bool M_Install(
    M_LANGUAGE *const language, const char *const code, const bool system)
{
    M_RUN *const run = language->run;
    FILE *const out = run->out;
    REPO_NETWORK *network;
    if (!M_OpenNetwork(language, &network)) {
        return false;
    }
    const char *id;
    const REPO *pack;
    const char *pack_name;
    bool matched;
    if (!M_Resolve(
            language, network, code, system, &id, &pack, &pack_name,
            &matched)) {
        return false;
    }
    language->target = id;
    M_Head(language);
    if (pack_name != nullptr) {
        fprintf(
            out, "%s: %s\n", system && matched ? "System language" : "Language",
            pack != nullptr ? M_GetPackTitle(pack, pack_name)
                            : pack_name + strlen(M_LANGUAGE_PREFIX));
    }

    REPO_PATCH_LIST list;
    Array_Init(&list, Arena_Allocator(run->arena));
    const RESULT added = Repo_AddPatch(network, id, &list);
    if (!IS_OK(added)) {
        M_PrintFailure(
            out, M_Format(run->arena, "Could not install %s", id), added);
        return false;
    }
    const REPO_PATCH *const target = list.items[list.count - 1];
    if (pack_name != nullptr) {
        bool has_files;
        const RESULT checked = Update_HasGameFiles(
            target, language->games, language->game_count, &has_files);
        if (!IS_OK(checked)) {
            M_PrintFailure(
                out, M_Format(run->arena, "Could not download %s", id),
                checked);
            fputs(m_Unchanged, out);
            return false;
        }
        if (!has_files) {
            fprintf(
                out, "%s has no files for %s.\n",
                M_GetLabel(target, run->arena),
                M_ListProducts(
                    language->games, language->game_count, run->arena));
            return false;
        }
    }
    const int32_t target_count = list.count;
    M_AddInstalled(language, network, &list);
    bool changed;
    if (!M_UpdateList(language, &list, target_count, &changed)) {
        return false;
    }
    if (language->kind == M_KIND_INSTALL) {
        fprintf(out, "Installed %s.\n", target->id);
        return true;
    }
    if (language->kind == M_KIND_UPDATE) {
        M_PrintUpdated(out, changed);
        return true;
    }
    return M_SaveLanguage(language, target->name + strlen(M_LANGUAGE_PREFIX));
}

// Runs the first section: sets up, updates or installs the language or the
// patch that the arguments name, and updates every installed patch.
// *first_run tells whether the language of the system was set.
static bool M_RunLanguage(M_RUN *const run, bool *const first_run)
{
    *first_run = false;
    M_LANGUAGE language = { .run = run };
    const RESULT listed =
        Update_ListInstalled(run->dir, run->arena, &language.installed);
    if (IS_OK(listed)) {
        language.game_count = M_PickGames(
            run->exe_dir, run->dir, &language.installed, run->arena,
            language.games);
    }
    const char *const name = run->args->name;
    const char *code = name;
    if (name == nullptr && IS_OK(listed)) {
        code = M_ReadLanguage(
            run->exe_dir, language.games, language.game_count, run->arena);
    }
    language.target = name;
    if (name == nullptr) {
        language.kind = code != nullptr ? M_KIND_UPDATE : M_KIND_SETUP;
    } else {
        language.kind =
            strchr(name, '/') != nullptr && !Patch_IsLanguagePack(name)
            ? M_KIND_INSTALL
            : M_KIND_SETUP;
    }
    if (!IS_OK(listed)) {
        M_Head(&language);
        M_PrintFailure(run->out, "Could not read the patches folder", listed);
        return false;
    }
    if (language.game_count == 0) {
        M_Head(&language);
        fputs("There is no opentouhou game beside othpatch.\n", run->out);
        return false;
    }
    if (code != nullptr && M_IsOriginal(code)) {
        return language.kind == M_KIND_UPDATE ? M_UpdateOriginal(&language)
                                              : M_SetOriginal(&language, false);
    }
    if (code != nullptr) {
        return M_Install(&language, code, false);
    }
    char system[PATCH_LANGUAGE_TAG_SIZE];
    Patch_GetLocaleCode(Locale_GetPreferred(), system);
    const bool set = system[0] != '\0' && M_IsOriginal(system)
        ? M_SetOriginal(&language, true)
        : M_Install(&language, system, true);
    *first_run = set;
    return set;
}

static void M_PrintLanguageLine(
    FILE *const out, const char *const code, const char *const title,
    const bool in_use, const bool installed)
{
    fprintf(
        out, "  %-8s %s%s\n", code, title,
        in_use          ? " (in use)"
            : installed ? " (installed)"
                        : "");
}

static int M_List(M_RUN *const run)
{
    FILE *const out = run->out;
    ARENA *const arena = run->arena;
    UPDATE_INSTALLED_LIST installed;
    const RESULT listed = Update_ListInstalled(run->dir, arena, &installed);
    if (!IS_OK(listed)) {
        M_PrintFailure(out, "Could not read the patches folder", listed);
        return 1;
    }
    const char *games[M_GAME_COUNT];
    const int32_t game_count =
        M_PickGames(run->exe_dir, run->dir, &installed, arena, games);
    REPO_NETWORK *network;
    const RESULT opened = Repo_Open(run->args->repo, arena, &network);
    if (!IS_OK(opened)) {
        M_PrintFailure(out, "Could not download the patch list", opened);
        return 1;
    }
    const REPO *const first = Repo_GetFirst(network);
    const char *const code =
        M_ReadLanguage(run->exe_dir, games, game_count, arena);
    const bool original = code != nullptr && M_IsOriginal(code);
    const char *const in_use = code != nullptr && !original
        ? M_FindLanguageOfCode(first, code)
        : nullptr;
    const JSON_MEMBER **packs;
    const int32_t pack_count = M_SortPacks(first, arena, &packs);
    fprintf(
        out, "Languages for %s:\n", M_ListProducts(games, game_count, arena));
    bool listed_original = false;
    for (int32_t i = 0; i < pack_count; i++) {
        const char *const pack_code = packs[i]->key + strlen(M_LANGUAGE_PREFIX);
        if (!listed_original
            && strcmp(PATCH_LANGUAGE_ORIGINAL, pack_code) < 0) {
            M_PrintLanguageLine(
                out, PATCH_LANGUAGE_ORIGINAL, M_ORIGINAL_TITLE, original,
                false);
            listed_original = true;
        }
        const char *const id =
            M_Format(arena, "%s/%s", first->id, packs[i]->key);
        M_PrintLanguageLine(
            out, pack_code, M_GetPackTitle(first, packs[i]->key),
            in_use != nullptr && strcmp(in_use, packs[i]->key) == 0,
            M_IsInstalled(&installed, id));
    }
    if (!listed_original) {
        M_PrintLanguageLine(
            out, PATCH_LANGUAGE_ORIGINAL, M_ORIGINAL_TITLE, original, false);
    }

    PATCH_SELECTION *selection;
    const RESULT selected =
        Patch_Select(run->dir, nullptr, nullptr, &selection);
    if (!IS_OK(selected)) {
        M_PrintFailure(out, "Could not read the patches folder", selected);
        return 1;
    }
    int32_t mods = 0;
    const int32_t count = Patch_GetEntryCount(selection);
    for (int32_t i = 0; i < count; i++) {
        const PATCH_ENTRY *const entry = Patch_GetEntry(selection, i);
        if (entry->role != PATCH_ROLE_MOD) {
            continue;
        }
        if (mods++ == 0) {
            fputs("Installed mods:\n", out);
        }
        const char *const title =
            Json_GetString(Json_Get(entry->patch_js, "title"), "");
        if (title[0] != '\0' && strcmp(title, entry->id) != 0) {
            fprintf(out, "  %s (%s)\n", title, entry->id);
        } else {
            fprintf(out, "  %s\n", entry->id);
        }
    }
    if (mods == 0) {
        fputs("Installed mods: none\n", out);
    }
    Patch_FreeSelection(selection);
    return 0;
}

// Returns the index of the installed patch name, -1 when no installed
// patch is name, or -2 when a bare name fits several.
static int32_t M_FindInstalled(
    const PATCH_SELECTION *const selection, const char *const name)
{
    const int32_t count = Patch_GetEntryCount(selection);
    int32_t found = -1;
    for (int32_t i = 0; i < count; i++) {
        const char *const id = Patch_GetEntry(selection, i)->id;
        if (strcmp(id, name) == 0) {
            return i;
        }
        const char *const slash = strchr(id, '/');
        if (strchr(name, '/') == nullptr && slash != nullptr
            && strcmp(slash + 1, name) == 0) {
            found = found == -1 ? i : -2;
        }
    }
    return found;
}

static void M_PrintAmbiguous(
    const PATCH_SELECTION *const selection, const char *const name,
    FILE *const out)
{
    const int32_t count = Patch_GetEntryCount(selection);
    int32_t matches = 0;
    for (int32_t i = 0; i < count; i++) {
        const char *const slash = strchr(Patch_GetEntry(selection, i)->id, '/');
        matches += slash != nullptr && strcmp(slash + 1, name) == 0;
    }
    fprintf(out, "%d installed patches are %s:", matches, name);
    for (int32_t i = 0, listed = 0; i < count; i++) {
        const char *const id = Patch_GetEntry(selection, i)->id;
        const char *const slash = strchr(id, '/');
        if (slash != nullptr && strcmp(slash + 1, name) == 0) {
            fprintf(out, "%s %s", listed++ > 0 ? "," : "", id);
        }
    }
    fputs(". Name one as <repo>/<patch>.\n", out);
}

static bool M_NeedsAny(const PATCH_ENTRY *const entry, const bool *const set)
{
    for (int32_t i = 0; i < entry->dependency_count; i++) {
        if (set[entry->dependencies[i]]) {
            return true;
        }
    }
    return false;
}

// Keeps language packs, so that removing a mod never removes the language
// that a game uses.
static void M_MarkRemovals(
    const PATCH_SELECTION *const selection, const int32_t index,
    bool *const removed)
{
    const int32_t count = Patch_GetEntryCount(selection);
    removed[index] = true;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int32_t i = 0; i < count; i++) {
            const PATCH_ENTRY *const entry = Patch_GetEntry(selection, i);
            if (removed[i] || Patch_IsLanguagePack(entry->id)) {
                continue;
            }
            bool needed_by_removed = false;
            bool needed_by_kept = false;
            for (int32_t j = 0; j < count; j++) {
                const PATCH_ENTRY *const user = Patch_GetEntry(selection, j);
                bool needs = false;
                for (int32_t k = 0; k < user->dependency_count; k++) {
                    needs = needs || (j != i && user->dependencies[k] == i);
                }
                needed_by_removed = needed_by_removed || (needs && removed[j]);
                needed_by_kept = needed_by_kept || (needs && !removed[j]);
            }
            if (needed_by_removed && !needed_by_kept) {
                removed[i] = true;
                changed = true;
            }
        }
    }
}

static bool M_RemoveFound(
    M_RUN *const run, const PATCH_SELECTION *const selection,
    const int32_t index)
{
    FILE *const out = run->out;
    const int32_t count = Patch_GetEntryCount(selection);
    const char *const target = Patch_GetEntry(selection, index)->id;
    bool *const removed =
        Arena_AllocArrZ(run->arena, (size_t)MAX(count, 1), sizeof(bool));
    removed[index] = true;
    for (int32_t i = 0; i < count; i++) {
        const PATCH_ENTRY *const entry = Patch_GetEntry(selection, i);
        if (i != index && M_NeedsAny(entry, removed)) {
            fprintf(
                out, "%s needs %s. Remove %s first.\n", entry->id, target,
                entry->id);
            return false;
        }
    }
    M_MarkRemovals(selection, index, removed);
    int32_t others = 0;
    for (int32_t i = 0; i < count; i++) {
        if (!removed[i]) {
            continue;
        }
        const char *const id = Patch_GetEntry(selection, i)->id;
        const RESULT result = Update_RemovePatch(run->dir, id);
        if (!IS_OK(result)) {
            M_PrintFailure(
                out, M_Format(run->arena, "Could not remove %s", id), result);
            return false;
        }
        if (run->args->verbose) {
            fprintf(out, "%s: removed\n", id);
        }
        others += i != index;
    }
    if (others == 0) {
        fprintf(out, "Removed %s.\n", target);
    } else {
        fprintf(
            out, "Removed %s and %d %s that only it needed.\n", target, others,
            others == 1 ? "patch" : "patches");
    }
    return true;
}

static int M_Remove(M_RUN *const run)
{
    FILE *const out = run->out;
    const char *const name = run->args->remove;
    PATCH_SELECTION *selection;
    const RESULT selected =
        Patch_Select(run->dir, nullptr, nullptr, &selection);
    const int32_t index =
        IS_OK(selected) ? M_FindInstalled(selection, name) : -1;
    M_StartSection(
        run, "Removing %s...",
        index >= 0 ? Patch_GetEntry(selection, index)->id : name);
    if (run->args->verbose) {
        fprintf(out, "Patches folder: %s\n", run->dir);
    }
    if (!IS_OK(selected)) {
        M_PrintFailure(out, "Could not read the patches folder", selected);
        return 1;
    }
    bool removed = false;
    if (index == -1) {
        fprintf(
            out,
            "There is no installed patch \"%s\".\n"
            "othpatch --list shows the installed mods.\n",
            name);
    } else if (index == -2) {
        M_PrintAmbiguous(selection, name, out);
    } else {
        removed = M_RemoveFound(run, selection, index);
    }
    Patch_FreeSelection(selection);
    return removed ? 0 : 1;
}

static RESULT M_Fetch(
    const char *const url, const FETCH_PROGRESS *const progress,
    FETCH *const fetch)
{
    *fetch = (FETCH) { .url = url };
    Fetch_Run(fetch, 1, progress);
    const RESULT result = fetch->result;
    fetch->result = OK;
    if (!IS_OK(result)) {
        Fetch_Release(fetch);
    }
    return result;
}

static RESULT M_FetchReleases(
    const char *const url, ARENA *const arena, JSON_VALUE **const out)
{
    *out = nullptr;
    FETCH fetch;
    MUST(M_Fetch(url, nullptr, &fetch));
    const RESULT parsed = Json_Parse(
        (const char *)fetch.data, fetch.size, Arena_Allocator(arena), out);
    Fetch_Release(&fetch);
    return parsed;
}

static void M_FormatVersion(
    const RELEASE_VERSION version, char out[GAME_VERSION_SIZE])
{
    snprintf(
        out, GAME_VERSION_SIZE, "%d.%d.%d", version.major, version.minor,
        version.patch);
}

// Downloads the package of release and checks its SHA-256 digest. name is
// the program that it holds. Fetch_Release releases *out.
static RESULT M_Download(
    const M_UPDATE *const update, const char *const name,
    const RELEASE *const release, FETCH *const out)
{
    FAIL_IF(
        release->sha256 == nullptr, "the index gives no SHA-256 digest of %s",
        release->package);
    char newest[GAME_VERSION_SIZE];
    M_FormatVersion(release->version, newest);
    M_BAR bar = {
        .run = update->run,
        .label = M_Format(update->run->arena, "%s %s", name, newest),
    };
    const FETCH_PROGRESS progress = {
        .received = M_ReportBytes,
        .context = &bar,
    };
    const RESULT fetched = M_Fetch(release->url, &progress, out);
    if (IS_OK(fetched) || bar.received > 0) {
        M_EndBytes(&bar, IS_OK(fetched));
    }
    MUST(fetched, "%s", release->package);
    char digest[SHA256_HEX_SIZE];
    Sha256_ComputeHex(out->data, out->size, digest);
    if (!Str_EqualIgnoreCase(digest, release->sha256)) {
        Fetch_Release(out);
        return FAIL(
            "%s has the SHA-256 digest %s instead of %s", release->package,
            digest, release->sha256);
    }
    return OK;
}

// Returns whether the answer on in, one line, is yes. An empty line is yes.
static bool M_ReadYes(FILE *const in)
{
    char line[64];
    if (fgets(line, sizeof(line), in) == nullptr) {
        return false;
    }
    char *end = line + strlen(line);
    while (end > line && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
    const char *start = line;
    while (isspace((unsigned char)*start)) {
        start++;
    }
    return start[0] == '\0' || Str_EqualIgnoreCase(start, "y")
        || Str_EqualIgnoreCase(start, "yes");
}

// Returns the release index, and fetches it on the first call. Prints the
// reason and returns nullptr when the fetch fails.
static const JSON_VALUE *M_GetReleases(M_UPDATE *const update)
{
    M_RUN *const run = update->run;
    if (!update->releases_fetched) {
        update->releases_fetched = true;
        const RESULT fetched =
            M_FetchReleases(run->args->releases, run->arena, &update->releases);
        if (!IS_OK(fetched)) {
            update->releases = nullptr;
            update->releases_error =
                M_Format(run->arena, "%s", fetched.message);
            Result_Ignore(fetched);
        }
    }
    if (update->releases == nullptr) {
        fprintf(
            run->out, "Could not check for updates: %s.\n",
            update->releases_error);
    }
    return update->releases;
}

// Finds the newest release of program, such as "oth06" or "othpatch", in
// the release index and compares it with installed. Returns whether the
// release is newer and has a package, and otherwise prints how the check
// ended. *failed tells whether the check failed.
static bool M_FindUpdate(
    M_UPDATE *const update, const char *const program,
    const RELEASE_VERSION installed, const char *const version,
    RELEASE *const out, bool *const failed)
{
    FILE *const stream = update->run->out;
    *failed = false;
    const JSON_VALUE *const index = M_GetReleases(update);
    if (index == nullptr) {
        *failed = true;
        return false;
    }
    const RESULT found =
        Release_FindNewest(index, program, update->platform->name, out);
    if (!IS_OK(found)) {
        M_PrintFailure(stream, "Could not check for updates", found);
        *failed = true;
        return false;
    }
    if (Release_CompareVersions(out->version, installed) <= 0) {
        fprintf(
            stream, "You already have the newest %s (%s).\n", program, version);
        return false;
    }
    char newest[GAME_VERSION_SIZE];
    M_FormatVersion(out->version, newest);
    if (out->url == nullptr) {
        fprintf(
            stream, "%s %s is out, but not for %s.\n", program, newest,
            update->platform->name);
        return false;
    }
    fprintf(stream, "%s %s is out (you have %s).\n", program, newest, version);
    return true;
}

// Returns whether to install a release: with --update, or when the answer
// on in is yes. Prints how the check ended otherwise.
static bool M_AskInstall(
    const M_UPDATE *const update, const char *const name,
    const char *const version)
{
    M_RUN *const run = update->run;
    if (run->args->update) {
        return true;
    }
    if (run->in == nullptr) {
        fprintf(
            run->out, "Kept %s %s. Run othpatch --update to install it.\n",
            name, version);
        return false;
    }
    fputs("Install it now? [Y/n] ", run->out);
    fflush(run->out);
    if (M_ReadYes(run->in)) {
        return true;
    }
    fprintf(
        run->out, "Kept %s %s. Run othpatch again to update.\n", name, version);
    return false;
}

static void M_PrintFailedInstall(
    const M_UPDATE *const update, const char *const name,
    const RELEASE *const release, const RESULT result)
{
    char newest[GAME_VERSION_SIZE];
    M_FormatVersion(release->version, newest);
    M_PrintFailure(
        update->run->out,
        M_Format(update->run->arena, "Could not update %s to %s", name, newest),
        result);
}

static void M_PrintOthpatchUpdated(
    FILE *const out, const char *const from, const RELEASE_VERSION to)
{
    char newest[GAME_VERSION_SIZE];
    M_FormatVersion(to, newest);
    fprintf(
        out, "Updated othpatch from %s to %s. The next run uses the new one.\n",
        from, newest);
}

static RESULT M_InstallGame(
    M_UPDATE *const update, const char *const game, const char *const name,
    const RELEASE *const release, bool *const othpatch_replaced)
{
    *othpatch_replaced = false;
    FETCH fetch;
    MUST(M_Download(update, name, release, &fetch));
    const RESULT result = Install_Package(
        update->platform, update->run->exe_dir, game, fetch.data, fetch.size,
        update->othpatch_is_release ? &update->othpatch_version : nullptr,
        update->run->arena, othpatch_replaced);
    Fetch_Release(&fetch);
    return result;
}

// Runs the section of one game: checks for a newer release, and installs
// it when --update or the answer on in says so. Returns whether the
// section ended well.
static bool M_UpdateGame(
    M_UPDATE *const update, const char *const game, const char *const path)
{
    FILE *const out = update->run->out;
    char name[GAME_NAME_SIZE];
    Game_GetProductName(game, name);
    M_StartSection(update->run, "Checking for %s updates...", name);
    char version[GAME_VERSION_SIZE];
    const RESULT asked = Game_GetVersion(path, version);
    if (!IS_OK(asked)) {
        M_PrintFailure(
            out,
            M_Format(
                update->run->arena, "Could not read the version of %s", name),
            asked);
        return false;
    }
    RELEASE_VERSION installed;
    if (!Release_ParseVersion(version, &installed)) {
        fprintf(
            out,
            "%s %s is a development build, so othpatch does not update it.\n",
            name, version);
        return true;
    }
    RELEASE release;
    bool failed;
    if (!M_FindUpdate(update, name, installed, version, &release, &failed)) {
        return !failed;
    }
    if (!M_AskInstall(update, name, version)) {
        return true;
    }
    char othpatch[GAME_VERSION_SIZE];
    M_FormatVersion(update->othpatch_version, othpatch);
    bool othpatch_replaced;
    const RESULT result =
        M_InstallGame(update, game, name, &release, &othpatch_replaced);
    if (!IS_OK(result)) {
        M_PrintFailedInstall(update, name, &release, result);
        return false;
    }
    char newest[GAME_VERSION_SIZE];
    M_FormatVersion(release.version, newest);
    fprintf(out, "Updated %s from %s to %s.\n", name, version, newest);
    if (othpatch_replaced) {
        M_PrintOthpatchUpdated(out, othpatch, update->othpatch_version);
    }
    return true;
}

static RESULT M_InstallOthpatch(
    const M_UPDATE *const update, const RELEASE *const release)
{
    FETCH fetch;
    MUST(M_Download(update, "othpatch", release, &fetch));
    const RESULT result = Install_Othpatch(
        update->platform, update->run->exe_dir, fetch.data, fetch.size,
        update->run->arena);
    Fetch_Release(&fetch);
    return result;
}

// Runs the section of othpatch: checks for a newer release, and installs
// it when --update or the answer on in says so. Returns whether the
// section ended well.
static bool M_UpdateOthpatch(M_UPDATE *const update, const char *const running)
{
    FILE *const out = update->run->out;
    M_StartSection(update->run, "Checking for othpatch updates...");
    if (!update->othpatch_is_release) {
        fprintf(
            out,
            "othpatch %s is a development build, so othpatch does not update "
            "it.\n",
            running);
        return true;
    }
    char version[GAME_VERSION_SIZE];
    M_FormatVersion(update->othpatch_version, version);
    RELEASE release;
    bool failed;
    if (!M_FindUpdate(
            update, "othpatch", update->othpatch_version, version, &release,
            &failed)) {
        return !failed;
    }
    if (!M_AskInstall(update, "othpatch", version)) {
        return true;
    }
    const RESULT result = M_InstallOthpatch(update, &release);
    if (!IS_OK(result)) {
        M_PrintFailedInstall(update, "othpatch", &release, result);
        return false;
    }
    update->othpatch_version = release.version;
    M_PrintOthpatchUpdated(out, version, release.version);
    return true;
}

// Runs the section of othpatch and then of each game beside it. Returns
// whether every section ended well.
static bool M_UpdateAll(M_RUN *const run, const char *const version)
{
    if (run->exe_dir == nullptr) {
        return true;
    }
    M_UPDATE update = {
        .run = run,
        .platform = Install_GetPlatform(),
    };
    update.othpatch_is_release =
        Release_ParseVersion(version, &update.othpatch_version);
    bool ok = M_UpdateOthpatch(&update, version);
    for (int32_t i = 0; i < M_GAME_COUNT; i++) {
        const char *const path = Install_GetGamePath(
            update.platform, run->exe_dir, m_Games[i], run->arena);
        if (File_Exists(path) && !M_UpdateGame(&update, m_Games[i], path)) {
            ok = false;
        }
    }
    return ok;
}

static int M_RunAll(M_RUN *const run, const char *const version)
{
    FILE *const out = run->out;
    bool first_run;
    bool ok = M_RunLanguage(run, &first_run);
    ok = M_UpdateAll(run, version) && ok;
    fputs(
        ok ? "\nAll done. Go yukkuri!\n" : "\nSomething failed, see above.\n",
        out);
    if (first_run) {
        fputs(
            "\n"
            "To use another language, run othpatch with its code, such as "
            "othpatch de.\n"
            "othpatch ja switches back to the original Japanese.\n"
            "othpatch --list shows every language.\n",
            out);
    }
    return ok ? 0 : 1;
}

int Main_Run(
    const int argc, char **const argv, const char *const version,
    const char *const exe_dir, FILE *const in, FILE *const out,
    const bool terminal)
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
        fprintf(out, "othpatch %s\n", version);
        return 0;
    }
    ARENA arena;
    Arena_Init(&arena, "othpatch", M_ARENA_SIZE);
    if (exe_dir != nullptr && !args.list) {
        Install_RemoveLeftovers(
            Install_GetPlatform(), exe_dir, m_Games, M_GAME_COUNT, &arena);
    }
    char *dir = nullptr;
    const RESULT found = M_FindDir(&args, exe_dir, &dir);
    if (!IS_OK(found)) {
        Arena_Free(&arena);
        return M_FailResult(out, "cannot find the patches folder", found);
    }
    M_RUN run = {
        .args = &args,
        .exe_dir = exe_dir,
        .dir = dir,
        .arena = &arena,
        .in = in,
        .out = out,
        .terminal = terminal,
    };
    int code = 1;
    if (args.remove != nullptr) {
        code = M_Remove(&run);
    } else {
        const RESULT started = Fetch_Init();
        if (!IS_OK(started)) {
            M_FailResult(out, "cannot start", started);
        } else if (args.list) {
            code = M_List(&run);
        } else {
            code = M_RunAll(&run, version);
        }
        Fetch_Shutdown();
    }
    Arena_Free(&arena);
    Mem_Free(dir);
    fflush(out);
    return code;
}
