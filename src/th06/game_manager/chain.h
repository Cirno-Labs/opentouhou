#pragma once

#include "core/base/result.h"

// Adds the update element (priority 4) and draw element (priority 2).
// Resets the stage frame count, advances the stage number and starts
// the stage. Outside SCENE_NEXT_STAGE, also initializes a new game.
// Returns failure when setup fails.
RESULT GameManager_Register(void);
// Removes both elements and the gameplay modules, releases the enemy
// scripts and ends the replay stage.
void GameManager_Remove(void);
