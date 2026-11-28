#pragma once

// game_over.c's own state, exposed for tests; nothing outside the
// game_over module sees this.

#include "th02/game_over.h"
#include "th02/hud.h"
#include "th02/wait.h"

typedef enum {
    GAME_OVER_ANIMATE,
    GAME_OVER_DELAY,
    GAME_OVER_KEY,
    GAME_OVER_REGISTER,
    GAME_OVER_CHOICE,
    GAME_OVER_DONE,
    GAME_OVER_COVER,
    GAME_OVER_REVEAL,
} GAME_OVER_PHASE;

typedef struct {
    const TH02_SESSION *session;
    WAIT wait;
    GAME_OVER_PHASE phase;
    GAME_OVER_EVENT event;
    uint8_t row;
    uint8_t input_state;
    bool yes;
    HUD_WIPE wipe;
} GAME_OVER;

// A writable view of game_over.c's private state, for tests.
extern GAME_OVER *const p_GameOver;
