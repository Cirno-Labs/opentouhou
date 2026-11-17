#pragma once

#include "shell/game.h"

// Describes the TH01 native game to shell/game.
extern const GAME_DESC g_GameDesc;

// Writes queued scores after the host iteration. Does nothing until game
// initialization completes, and after shutdown.
void Th01_FlushSaves(void);
