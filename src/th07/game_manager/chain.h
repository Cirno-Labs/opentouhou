#pragma once

#include "core/base/result.h"

// Runs stage setup, score counting, pause gating, demo timing and the
// calcs that slow playback and slow mode drop. Unported modules have
// explicit stand-ins. The depth clear and the loading screen come with
// their presentation modules.

// Adds the update element (priority 2) and draw element (priority 2).
// Adding resets the game speed to 1, advances the stage number and
// starts the stage. Outside SCENE_NEXT_STAGE, also initializes a new
// game, with the random numbers that the original's checksum setup
// draws. Returns failure when setup fails.
RESULT GameManager_Register(void);

// Removes both elements and the gameplay modules, releases the enemy
// scripts and ends the replay stage. Caps the score at 999,999,999 tens
// and shows it in full.
void GameManager_Remove(void);
