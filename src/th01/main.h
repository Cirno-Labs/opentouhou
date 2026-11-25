#pragma once

#include "th01/hooks.h"

#include "shell/game.h"

// Describes the TH01 native game to shell/game.
extern const GAME_DESC g_GameDesc;

typedef enum {
    TH01_SCENE_LOGO,
    TH01_SCENE_OPENING,
    TH01_SCENE_GAMEPLAY,
    TH01_SCENE_SCORE,
    TH01_SCENE_ENDING,
    TH01_SCENE_SWITCH,
    TH01_SCENE_ENDING_SCORE,
} TH01_SCENE;

extern TH01_SCENE g_Th01Scene;
