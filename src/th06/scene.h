#pragma once

#include "core/base/result.h"

typedef enum {
    SCENE_NONE = -1,
    SCENE_BOOT = 0,
    SCENE_TITLE = 1,
    SCENE_GAMEPLAY,
    SCENE_NEXT_STAGE,
    SCENE_QUIT,
    SCENE_RESET,
    SCENE_SCORES,
    SCENE_GAME_RESULT,
    SCENE_REPLAY_END,
    SCENE_MUSIC_ROOM,
    SCENE_ENDING,
} SCENE;

// Adds the element that runs first in every calc: it updates g_Buttons
// from the tick's input, then sets up the requested scene.
RESULT Scene_Register(void);
// Requests a switch at the start of the next calc.
void Scene_Request(SCENE scene);
SCENE Scene_Get(void);
