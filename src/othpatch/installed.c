#include "othpatch/context.h"

#include "othpatch/fetch.h"
#include "othpatch/game.h"

#include "core/io/file.h"
#include "core/io/path.h"
#include "core/patch/language.h"
#include "core/patch/select.h"

#include <string.h>

// Holds the state of a removal: the patch that it removes, and how many
// other patches it removed.
typedef struct {
    APP_CONTEXT *run;
    const char *target;
    int32_t others;
} M_REMOVAL;

static bool M_IsInstalled(
    const PATCH_MANAGER *const manager, const char *const id)
{
    for (int32_t i = 0; i < PatchManager_GetCount(manager); i++) {
        if (strcmp(PatchManager_Get(manager, i)->id, id) == 0) {
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
    ARENA *const arena, const PATCH_SELECTION *const selection,
    const char *const game)
{
    for (int32_t i = 0; i < Patch_GetEntryCount(selection); i++) {
        const char *const dir = Patch_GetEntry(selection, i)->dir;
        if (File_Exists(Path_Join(Arena_Allocator(arena), dir, game))) {
            return true;
        }
    }
    return false;
}

// Opens the patch manager for the games whose files to download: the games
// beside othpatch and the games that an installed patch has a folder for.
RESULT Installed_Open(APP_CONTEXT *const run)
{
    PATCH_SELECTION *selection;
    MUST(Patch_Select(run->dir, nullptr, nullptr, &selection));
    for (int32_t i = 0; i < APP_GAME_COUNT; i++) {
        if (M_IsBeside(run->arena, run->exe_dir, g_Games[i])
            || M_IsDownloaded(run->arena, selection, g_Games[i])) {
            run->games[run->game_count++] = g_Games[i];
        }
    }
    Patch_FreeSelection(selection);
    run->manager = PatchManager_Open(
        run->dir, run->args->repo, run->games, run->game_count, Fetch_Run);
    return OK;
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

int Installed_List(APP_CONTEXT *const run)
{
    FILE *const out = run->out;
    ARENA *const arena = run->arena;
    const RESULT opened = Installed_Open(run);
    if (!IS_OK(opened)) {
        Console_Fail(
            out, opened, "Could not read the patches folder: %s.",
            opened.message);
        return 1;
    }
    const PATCH_LISTED *listed;
    int32_t listed_count;
    const RESULT fetched =
        PatchManager_ListPatches(run->manager, &listed, &listed_count);
    if (!IS_OK(fetched)) {
        Console_Fail(
            out, fetched, "Could not download the patch list: %s.",
            fetched.message);
        return 1;
    }
    const char *const code = StepPatches_ReadLanguage(run);
    const bool original = code != nullptr && StepPatches_IsOriginal(code);
    const PATCH_LISTED *const in_use = code != nullptr && !original
        ? StepPatches_FindLanguageOfCode(listed, listed_count, code)
        : nullptr;
    const PATCH_LISTED **packs;
    const int32_t pack_count =
        StepPatches_SortPacks(listed, listed_count, arena, &packs);
    fprintf(out, "Languages for %s:\n", StepPatches_ListProducts(run));
    bool listed_original = false;
    for (int32_t i = 0; i < pack_count; i++) {
        const char *const pack_code = StepPatches_GetPackCode(packs[i]);
        if (!listed_original
            && strcmp(PATCH_LANGUAGE_ORIGINAL, pack_code) < 0) {
            M_PrintLanguageLine(
                out, PATCH_LANGUAGE_ORIGINAL, APP_ORIGINAL_TITLE, original,
                false);
            listed_original = true;
        }
        M_PrintLanguageLine(
            out, pack_code, StepPatches_GetPackTitle(packs[i]),
            in_use == packs[i], M_IsInstalled(run->manager, packs[i]->id));
    }
    if (!listed_original) {
        M_PrintLanguageLine(
            out, PATCH_LANGUAGE_ORIGINAL, APP_ORIGINAL_TITLE, original, false);
    }

    int32_t mods = 0;
    for (int32_t i = 0; i < PatchManager_GetCount(run->manager); i++) {
        const PATCH_ENTRY *const entry = PatchManager_Get(run->manager, i);
        if (entry->role != PATCH_ROLE_MOD) {
            continue;
        }
        if (mods++ == 0) {
            fputs("Installed mods:\n", out);
        }
        if (strcmp(entry->title, entry->id) != 0) {
            fprintf(out, "  %s (%s)\n", entry->title, entry->id);
        } else {
            fprintf(out, "  %s\n", entry->id);
        }
    }
    if (mods == 0) {
        fputs("Installed mods: none\n", out);
    }
    return 0;
}

// Returns the index of the installed patch name, -1 when no installed
// patch is name, or -2 when a bare name fits several.
static int32_t M_FindInstalled(
    const PATCH_MANAGER *const manager, const char *const name)
{
    int32_t found = -1;
    for (int32_t i = 0; i < PatchManager_GetCount(manager); i++) {
        const char *const id = PatchManager_Get(manager, i)->id;
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
    const PATCH_MANAGER *const manager, const char *const name, FILE *const out)
{
    const int32_t count = PatchManager_GetCount(manager);
    int32_t matches = 0;
    for (int32_t i = 0; i < count; i++) {
        const char *const slash = strchr(PatchManager_Get(manager, i)->id, '/');
        matches += slash != nullptr && strcmp(slash + 1, name) == 0;
    }
    fprintf(out, "%d installed patches are %s:", matches, name);
    for (int32_t i = 0, listed = 0; i < count; i++) {
        const char *const id = PatchManager_Get(manager, i)->id;
        const char *const slash = strchr(id, '/');
        if (slash != nullptr && strcmp(slash + 1, name) == 0) {
            fprintf(out, "%s %s", listed++ > 0 ? "," : "", id);
        }
    }
    fputs(". Name one as <repo>/<patch>.\n", out);
}

static void M_ReportRemoved(void *const user, const char *const id)
{
    M_REMOVAL *const removal = user;
    if (removal->run->args->verbose) {
        fprintf(removal->run->out, "%s: removed\n", id);
    }
    removal->others += strcmp(id, removal->target) != 0;
}

static bool M_RemoveFound(APP_CONTEXT *const run, const int32_t index)
{
    FILE *const out = run->out;
    const PATCH_MANAGER *const manager = run->manager;
    M_REMOVAL removal = {
        .run = run,
        .target = Console_Format(
            run->arena, "%s", PatchManager_Get(manager, index)->id),
    };
    for (int32_t i = 0; i < PatchManager_GetCount(manager); i++) {
        const PATCH_ENTRY *const entry = PatchManager_Get(manager, i);
        for (int32_t j = 0; j < entry->dependency_count; j++) {
            if (entry->dependencies[j] == index) {
                fprintf(
                    out, "%s needs %s. Remove %s first.\n", entry->id,
                    removal.target, entry->id);
                return false;
            }
        }
    }
    const PATCH_EVENTS events = {
        .user = &removal,
        .patch_removed = M_ReportRemoved,
    };
    const RESULT result =
        PatchManager_Remove(run->manager, removal.target, &events);
    if (!IS_OK(result)) {
        fprintf(out, "Could not remove %s.\n", result.message);
        Result_Ignore(result);
        return false;
    }
    if (removal.others == 0) {
        fprintf(out, "Removed %s.\n", removal.target);
    } else {
        fprintf(
            out, "Removed %s and %d %s that only it needed.\n", removal.target,
            removal.others, removal.others == 1 ? "patch" : "patches");
    }
    return true;
}

int Installed_Remove(APP_CONTEXT *const run)
{
    FILE *const out = run->out;
    const char *const name = run->args->remove;
    const RESULT opened = Installed_Open(run);
    const int32_t index =
        IS_OK(opened) ? M_FindInstalled(run->manager, name) : -1;
    Console_StartSection(
        run, "Removing %s...",
        index >= 0 ? PatchManager_Get(run->manager, index)->id : name);
    if (run->args->verbose) {
        fprintf(out, "Patches folder: %s\n", run->dir);
    }
    if (!IS_OK(opened)) {
        Console_Fail(
            out, opened, "Could not read the patches folder: %s.",
            opened.message);
        return 1;
    }
    if (index == -1) {
        fprintf(
            out,
            "There is no installed patch \"%s\".\n"
            "othpatch --list shows the installed mods.\n",
            name);
        return 1;
    }
    if (index == -2) {
        M_PrintAmbiguous(run->manager, name, out);
        return 1;
    }
    return M_RemoveFound(run, index) ? 0 : 1;
}
