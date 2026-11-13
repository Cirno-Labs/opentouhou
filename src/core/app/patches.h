// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/patch/stack.h"

// Returns the patches of the game, from the shared patches folder, in the
// order in which they apply. core/app opens them before the game's init and
// closes them after its shutdown. Returns nullptr when the game takes no
// patches, when the [patches] setting enabled is false, or when the folder
// cannot be read. [patches] disabled lists patch ids, separated by commas,
// that are left out.
const PATCH_STACK *App_GetPatches(void);
