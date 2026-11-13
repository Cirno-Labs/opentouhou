// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/result.h"
#include "core/base/types.h"

// Finds the thcrap patches in a patches folder and puts them in the order
// in which they apply. The folder has the layout of thcrap's repos/
// folder: one directory per repository, and in it one directory per patch
// with its patch.js, so that a patch is <folder>/<repo>/<patch>/ and has
// the id "<repo>/<patch>".
//
// A patch applies after every patch that it depends on, so that its files
// replace theirs. Patches with no order between them apply in byte order of
// their ids. A dependency "<repo>/<patch>" names that patch. A bare
// "<patch>" names the patch with that name in the same repository, or else
// in the first repository, in byte order, that has one. This is how
// thcrap's configurator resolves dependencies.
//
// These patches are left out, each with a log line:
// - a disabled patch;
// - a patch whose patch.js does not read or parse as an object;
// - a patch whose supported_games does not list the game;
// - a patch with a dependency that is missing or left out;
// - a patch that depends on itself through other patches.
//
// File names in a patch are case-insensitive. In each patch, a file has a
// build-specific name with the game's build after the first part of its
// name: "msg1.v1.02h.dat" for "msg1.dat", "th06.v1.02h.js" for "th06.js".
//
// Example:
// PATCH_STACK *stack;
// MUST(Patch_OpenStack(dir, "th06", &game, nullptr, 0, &stack));
// for (int32_t i = 0; i < Patch_GetCount(stack); i++) {
//     LOG_INFO("%s", Patch_Get(stack, i)->id);
// }
// Patch_CloseStack(stack);

typedef struct PATCH_STACK PATCH_STACK;

// Selects how a path of the game maps to a file in the game directory of a
// patch, <patch>/<game id>/.
typedef enum {
    // The file is named after the last component of the path, as for th06,
    // whose archives hold every file in one flat list.
    PATCH_LAYOUT_FLAT,
    // The file has the whole path below the game directory, as for th07,
    // whose archives keep their directories.
    PATCH_LAYOUT_PATHS,
} PATCH_LAYOUT;

// Tells what a file in the game directory of a patch does to the game.
typedef enum {
    // The file replaces the game's file of the same name.
    PATCH_FILE_DATA,
    // The file replaces the game's file and changes what happens in play,
    // so the stack changes gameplay.
    PATCH_FILE_GAMEPLAY,
    // The file is not used: the game cannot take that kind of file from a
    // patch yet.
    PATCH_FILE_IGNORED,
} PATCH_FILE_KIND;

// Describes a game to its patches.
typedef struct {
    // thcrap's name for the build of the original that the game ports, such
    // as "v1.02h".
    const char *build;
    PATCH_LAYOUT layout;
    // Returns the kind of a file in the game directory of a patch, given by
    // its path below that directory in lowercase and without the build,
    // such as "ecldata1.ecl". It is never called for a .jdiff file. A
    // nullptr classify makes every file PATCH_FILE_DATA.
    PATCH_FILE_KIND (*classify)(const char *name);
} PATCH_GAME;

typedef struct {
    // The id, "<repo>/<patch>".
    const char *id;
    // The title from patch.js, or the id when patch.js has none.
    const char *title;
    // The directory of the patch, ending with '/'.
    const char *dir;
    // The paths on disk of the font files of the patch: the members of the
    // patch.js object "fonts" whose value is true, each joined to dir.
    const char *const *fonts;
    int32_t font_count;
} PATCH_INFO;

// Opens the patches in dir that apply to the game game_id, which is the
// thcrap game id and the GAME_DESC id, such as "th06". disabled lists
// disabled_count patch ids that are left out. A dir that does not exist
// gives an empty stack. Reports failure only when dir exists and cannot be
// listed; *out is then nullptr. The stack keeps a copy of game.
RESULT Patch_OpenStack(
    const char *dir, const char *game_id, const PATCH_GAME *game,
    const char *const *disabled, int32_t disabled_count, PATCH_STACK **out);

// Releases a stack. A nullptr stack does nothing.
void Patch_CloseStack(PATCH_STACK *stack);

// Returns the number of patches in the stack, 0 for a nullptr stack.
int32_t Patch_GetCount(const PATCH_STACK *stack);

// Returns the patch at index. Index 0 applies first. An index out of range
// is fatal.
const PATCH_INFO *Patch_Get(const PATCH_STACK *stack, int32_t index);

// Returns whether a patch of the stack has a file of the kind
// PATCH_FILE_GAMEPLAY.
bool Patch_ChangesGameplay(const PATCH_STACK *stack);
