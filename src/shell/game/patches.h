#pragma once

#include "core/patch/stack.h"

// Returns the patches of the game, from the shared patches folder, in the
// order in which they apply. shell/game opens them before the game's init and
// closes them after its shutdown. core/patch/select.h selects them from the
// [patches] settings, which Patch_ReadChoice reads from config.ini, and the
// preferred locales. Returns nullptr when the game takes no patches, or
// when the folder cannot be read.
const PATCH_STACK *App_GetPatches(void);
