#include "othpatch/context.h"

#include "othpatch/fetch.h"
#include "othpatch/game.h"

#include "core/patch/select.h"

// Opens the patch manager for the games whose files to download: the games
// beside othpatch and the games that an installed patch has a folder for.
RESULT App_OpenPatches(APP_CONTEXT *const run)
{
    PATCH_SELECTION *selection;
    MUST(Patch_Select(run->dir, nullptr, nullptr, &selection));
    Game_FindBeside(
        run->games, &run->game_count, run->arena, run->exe_dir, run->out);
    for (int32_t i = 0; i < Patch_GetEntryCount(selection); i++) {
        Game_FindDownloaded(
            run->games, &run->game_count, run->arena,
            Patch_GetEntry(selection, i)->dir, run->out);
    }
    Patch_FreeSelection(selection);
    run->manager = PatchManager_Open(
        run->dir, run->args->repo, run->games, run->game_count, Fetch_Run);
    return OK;
}
