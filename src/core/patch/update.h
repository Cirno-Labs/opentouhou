#pragma once

#include "core/base/arena.h"
#include "core/base/array.h"
#include "core/base/result.h"
#include "core/patch/fetch.h"
#include "core/patch/manager.h"
#include "core/patch/repo.h"

// Keeps the files of patches in a patches folder, as manager.h describes.

// Holds one patch in a patches folder.
typedef struct {
    // Holds the id, "<repository id>/<patch name>".
    const char *id;
    // Tells whether patch.js lets the patch update, which it does unless
    // its "update" is false.
    bool update;
} PATCH_INSTALLED;

typedef ARRAY(PATCH_INSTALLED) PATCH_INSTALLED_LIST;

// Brings the files of a patch in the patches folder dir up to date with
// its files.js, which comes from the first server of the patch that gives
// it, for the games that games names. Paths with an empty, "." or ".."
// part, a '\' or a ':' are left out. A .part file from an earlier update
// that stopped is replaced or removed. Reports to events with entry as
// the patch, except patch_done; events may be nullptr.
RESULT PatchUpdate_Patch(
    const char *dir, const PATCH_REMOTE *patch, const char *const *games,
    int32_t game_count, PATCH_FETCH_FN fetch, const PATCH_EVENTS *events,
    const PATCH_ENTRY *entry, PATCH_COUNTS *counts);

// Fetches the files.js of a patch as PatchUpdate_Patch does, and sets *out
// to whether it lists a file below "<game>/" for one of games.
RESULT PatchUpdate_HasGameFiles(
    const PATCH_REMOTE *patch, const char *const *games, int32_t game_count,
    PATCH_FETCH_FN fetch, bool *out);

// Lists the patches of the patches folder dir, each <repository>/<patch>/
// with a patch.js, in byte order of their ids. The list and its strings
// come from arena. A missing folder has no patches. A patch.js that does
// not parse lets the patch update.
RESULT PatchUpdate_ListInstalled(
    const char *dir, ARENA *arena, PATCH_INSTALLED_LIST *out);

// Removes a patch from the patches folder dir as PatchManager_Remove
// describes. Without patch.js, Patch_Select no longer reads what is left.
RESULT PatchUpdate_RemovePatch(const char *dir, const char *id);
