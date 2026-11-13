#include "othpatch/context.h"

#include "othpatch/fetch.h"
#include "othpatch/game.h"

#include "core/io/file.h"
#include "core/io/path.h"
#include "core/patch/select.h"

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
RESULT App_OpenPatches(APP_CONTEXT *const run)
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
