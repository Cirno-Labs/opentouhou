#pragma once

// credits.c's own state, exposed for tests; nothing outside the ending/
// module sees this.

#include "th02/ending/credits.h"
#include "th02/ending/render.h"
#include "th02/ending/text.h"
#include "th02/wait.h"

typedef enum {
    CREDITS_BEGIN,
    CREDITS_MEASURE,
    CREDITS_TITLE,
    CREDITS_MOVE,
    CREDITS_HEADING,
    CREDITS_TEXT,
    CREDITS_PICTURE,
    CREDITS_ROTATION,
    CREDITS_PICTURE_HOLD,
    CREDITS_BLACK_OUT,
    CREDITS_FADE,
    CREDITS_DONE,
} CREDITS_PHASE;

typedef struct {
    const ENDING_ASSETS *assets;
    WAIT wait;
    ENDING_ROTATION rotation;
    ENDING_TEXT text;
    CREDITS_PHASE phase;
    CREDITS_PHASE after_measure;
    int16_t title_top;
    uint8_t image;
    uint8_t title_frame;
} CREDITS;

// A writable view of credits.c's private state, for tests.
extern CREDITS *const p_Credits;
