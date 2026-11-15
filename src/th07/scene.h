#pragma once

#include "core/base/result.h"

typedef enum {
    SCENE_QUIT = -1,
    SCENE_BOOT = 0,
    SCENE_TITLE = 1,
    SCENE_GAMEPLAY,
    SCENE_NEXT_STAGE,
    SCENE_RESET,
    SCENE_SCORES,
    SCENE_GAME_RESULT,
    SCENE_REPLAY_END,
    SCENE_MUSIC_ROOM,
    SCENE_ENDING,
    // Starts the game again: from stage 1, or from the stage being played
    // in a practice and in Extra and Phantasm.
    SCENE_RETRY,
    // Plays the stage being played again. Nothing in the original requests
    // it.
    SCENE_RESTART_STAGE,
    // Goes on to the next stage as SCENE_NEXT_STAGE does. Nothing in the
    // original requests it.
    SCENE_NEXT_STAGE_AGAIN,
} SCENE;

// Adds the element that runs first in every calc: it updates g_Buttons
// from the tick's input, then sets up the requested scene. Requests start
// at SCENE_QUIT, which boot ignores.
RESULT Scene_Register(void);

// Requests a switch at the start of the next calc.
void Scene_Request(SCENE scene);
SCENE Scene_Get(void);

// Returns the scene that the last switch left, which the title reads.
SCENE Scene_GetPrevious(void);

// Returns the scene last requested. While a switch sets a scene up, this
// is the scene being set up, and Scene_Get returns the scene before it.
SCENE Scene_GetRequested(void);
