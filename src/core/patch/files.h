// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/json/types.h"
#include "core/patch/stack.h"
#include "core/vfs/types.h"

// Reads the files of a patch stack. A game path maps to a file in the game
// directory of each patch as the stack's PATCH_LAYOUT says. A later patch
// beats an earlier one, and in each patch the build-specific name beats the
// plain one, as in thcrap.

// Builds a VFS source with the game files of the patches. For a game path,
// it has the file of the last patch that has one. .jdiff files and files of
// the kind PATCH_FILE_IGNORED are not in it. The source is named "patches"
// and finds files through the stack's index, without access to the disk;
// it reads a file when the file is opened. The stack must outlive the
// source.
VFS_SOURCE *Patch_CreateSource(
    const PATCH_STACK *stack, const ALLOCATOR *allocator);

// Selects the directory of a patch that a JSON file is in.
typedef enum {
    // The patch directory itself, as for stringdefs.js and themes.js.
    PATCH_ROOT,
    // The game directory of the patch, as for spells.js and stringlocs.js.
    PATCH_GAME_DIR,
} PATCH_SCOPE;

// Merges the JSON file name of every patch into one tree, as thcrap merges
// the JSON files of a stack: in the order in which the patches apply, and in
// each patch the plain name before the build-specific one, each merged over
// the tree so far with Json_Merge. A file that is not an object replaces
// the tree. Returns nullptr when no patch has the file, or for a nullptr
// stack. A file that does not read or parse is left out, with a log line.
// The tree comes from allocator, which is meant to be an arena; the merged
// files stay in it too.
JSON_VALUE *Patch_LoadJson(
    const PATCH_STACK *stack, PATCH_SCOPE scope, const char *name,
    const ALLOCATOR *allocator);

// Returns thcrap's run configuration of the game: global.js, then
// "<game id>.js", each merged over the stack as Patch_LoadJson merges it
// from PATCH_ROOT, with the second merged over the first. Returns nullptr
// when no patch has either file, or for a nullptr stack.
JSON_VALUE *Patch_LoadSettings(
    const PATCH_STACK *stack, const ALLOCATOR *allocator);

// Returns the merged jdiff of the game file at path: the tree that
// Patch_LoadJson returns for "<name>.jdiff" in the game directory, where
// name is the file that path maps to.
JSON_VALUE *Patch_LoadDiff(
    const PATCH_STACK *stack, const char *path, const ALLOCATOR *allocator);

// Returns whether a patch replaces the game file at path, through the
// source of Patch_CreateSource, or has a jdiff for it. A nullptr stack
// returns false.
bool Patch_Touches(const PATCH_STACK *stack, const char *path);
