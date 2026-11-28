#pragma once

#include "th01/score/score.h"

typedef enum {
    SCORE_FADE_WAIT,
    SCORE_ENTRY_INPUT,
    SCORE_ENTRY_DELAY,
    SCORE_RELEASE,
    SCORE_DONE,
} SCORE_PHASE;

typedef struct {
    PALETTE_STATE palette;
    PALETTE_FADE fade;
    VSYNC_WAIT wait;
    SCORE_CONTEXT context;
    SCORE_PHASE phase;
    int32_t rank;
    int32_t place;
    int32_t row;
    int32_t column;
    int32_t cursor;
    int32_t inputs;
    char slots[8][9];
} SCORE_SCENE;

// Writable views of the saved tables and registration scene for tests.
extern SCORE_SAVES *const p_Saves;
extern SCORE_SCENE *const p_Registration;
