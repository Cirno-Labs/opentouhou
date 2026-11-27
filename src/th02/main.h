#pragma once

#include "th02/hooks.h"
#include "shell/game.h"

typedef enum {
    TH02_SCENE_OPENING,
    TH02_SCENE_GAMEPLAY,
    TH02_SCENE_ENDING,
    TH02_SCENE_REGISTRATION,
    TH02_SCENE_GAME_OVER,
    TH02_SCENE_BONUS,
    TH02_SCENE_STAGE_LEAVE,
    TH02_SCENE_ENDING_REGISTRATION,
} TH02_SCENE;

typedef struct {
    TH02_SCENE scene;
    int32_t frame;
    uint8_t stage;
    bool reduce_effects;
    bool ending_program;
} TH02_VIEW;

// The game's description; VSync phase survives program handoffs.
extern const GAME_DESC g_GameDesc;

// Reports whether a tick or shutdown failed since initialization.
bool Th02_Failed(void);

// Lets each tick sample the live wall clock and local
// date instead of leaving them to the caller. Only the real program's
// entry point calls this; a test, a replay or a capture never does.
void Th02_EnableLiveClock(void);

// Copies the state replays observe.
void Th02_View(TH02_VIEW *out);
