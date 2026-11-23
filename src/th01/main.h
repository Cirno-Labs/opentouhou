#pragma once

#include "th01/hooks.h"
#include "th01/opening.h"
#include "th01/score.h"
#include "th01/session.h"

#include "shell/game.h"

// Describes the TH01 native game to shell/game.
extern const GAME_DESC g_GameDesc;

// Writes queued scores after the host iteration. Does nothing until game
// initialization completes, and after shutdown.
void Th01_FlushSaves(void);

typedef struct GAMEPLAY GAMEPLAY;
typedef struct ENDING ENDING;

typedef enum {
    TH01_SCENE_LOGO,
    TH01_SCENE_OPENING,
    TH01_SCENE_GAMEPLAY,
    TH01_SCENE_SCORE,
    TH01_SCENE_ENDING,
} TH01_SCENE;

// Gives the running scene and the state that the harness reads. score is
// valid in TH01_SCENE_SCORE only; the others are valid after init.
typedef struct {
    TH01_SCENE scene;
    const OPENING *opening;
    const GAMEPLAY *gameplay;
    const SCORE_SCENE *score;
    const ENDING *ending;
} TH01_VIEW;

// Fills out with the running scene and its state.
void Th01_View(TH01_VIEW *out);
