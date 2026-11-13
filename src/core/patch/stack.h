#pragma once

#include "core/base/result.h"
#include "core/base/types.h"
#include "core/patch/select.h"

// Opens the thcrap patches of a patches folder in the order in which they
// apply. The folder has the layout of thcrap's repos/ folder: one
// directory per repository, and in it one directory per patch with its
// patch.js, so that a patch is <folder>/<repo>/<patch>/ and has the id
// "<repo>/<patch>".
//
// core/patch/select.h decides which patches load and their order.
//
// File names in a patch are case-insensitive. In each patch, a file has a
// build-specific name with the game's build after the first part of its
// name: "msg1.v1.02h.dat" for "msg1.dat", "th06.v1.02h.js" for "th06.js".
//
// Example:
// PATCH_STACK *stack;
// MUST(Patch_OpenStack(dir, "th06", &game, nullptr, &stack));
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

// Opens the patches in dir that Patch_Select selects for game_id and
// choice, and logs the description of the selection, with a warning when
// the language is missing. game_id is the thcrap game id and the GAME_DESC
// id, such as "th06". A missing dir gives an empty stack. Reports failure
// only when dir exists and cannot be listed. The stack keeps a copy of
// game.
RESULT Patch_OpenStack(
    const char *dir, const char *game_id, const PATCH_GAME *game,
    const PATCH_CHOICE *choice, PATCH_STACK **out);

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
