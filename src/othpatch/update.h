// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "othpatch/repo.h"

#include "core/base/arena.h"
#include "core/base/array.h"
#include "core/base/result.h"

// Keeps patches in a patches folder, in the layout of thcrap's repos/
// folder that core/patch reads: <folder>/<repository id>/repo.js, and
// <folder>/<repository id>/<patch name>/ with the patch.js, the files.js
// and the files of the patch.

// Holds what an update did to the files of a patch.
typedef struct {
    int32_t downloaded;
    int32_t up_to_date;
    int32_t removed;
} UPDATE_COUNTS;

// Holds one patch in a patches folder.
typedef struct {
    // Holds the id, "<repository id>/<patch name>".
    const char *id;
    // Tells whether patch.js lets the patch update, which it does unless
    // its "update" is false.
    bool update;
} UPDATE_INSTALLED;

typedef ARRAY(UPDATE_INSTALLED) UPDATE_INSTALLED_LIST;

// Brings the files of a patch in the patches folder dir up to date with
// its files.js, which comes from the first server of the patch that gives
// it. Of the files that files.js lists, the update keeps the files that
// every game shares, whose paths have no '/', and the files of the games
// that games names, whose paths start with "<game>/". Paths with an empty,
// "." or ".." part, a '\' or a ':' are left out.
//
// A file whose CRC-32 matches files.js is up to date. Every other file is
// fetched with "?crc32=<hex>" after its URL, from each server of the patch
// in order until one gives it with the CRC-32 of files.js, and written to
// "<file>.part". When every file has arrived, the .part files are renamed
// into place, the files that files.js marks as deleted with null are
// removed, and repo.js, files.js and patch.js are written last. When a
// file does not arrive, the update reports failure and changes no file of
// the patch. A .part file from an earlier update that stopped is replaced
// or removed.
RESULT Update_Patch(
    const char *dir, const REPO_PATCH *patch, const char *const *games,
    int32_t game_count, UPDATE_COUNTS *counts);

// Lists the patches of the patches folder dir, each <repository>/<patch>/
// with a patch.js, in byte order of their ids. The list and its strings
// come from arena. A missing folder has no patches. A patch.js that does
// not parse lets the patch update.
RESULT Update_ListInstalled(
    const char *dir, ARENA *arena, UPDATE_INSTALLED_LIST *out);

// Removes a patch from the patches folder dir: the files that its files.js
// lists, with their .part files, its files.js and patch.js, and then each
// folder of the patch that is empty. Other files stay. Without patch.js,
// core/patch no longer reads what is left.
RESULT Update_RemovePatch(const char *dir, const char *id);
