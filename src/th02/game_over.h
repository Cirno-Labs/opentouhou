#pragma once

#include "th02/session.h"

typedef enum {
    GAME_OVER_WAIT,
    GAME_OVER_NAME,
    GAME_OVER_RESUME,
    GAME_OVER_EXIT,
} GAME_OVER_EVENT;

void GameOver_Init(void);

// Starts the game over screen for session, which must outlive it.
void GameOver_Begin(const TH02_SESSION *session);

// Runs one frame. The continue prompt needs a new press, with Down over Up
// and Cancel over confirmation.
GAME_OVER_EVENT GameOver_Update(uint16_t keys);

// Shows the continue prompt after registration, except in Extra or after
// three credits.
void GameOver_Resume(void);
