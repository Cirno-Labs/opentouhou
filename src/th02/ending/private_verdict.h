#pragma once

// verdict.c's own state, exposed for tests; nothing outside the ending/
// module sees this.

#include "th02/ending/render.h"
#include "th02/ending/verdict.h"
#include "th02/wait.h"

typedef enum {
    VERDICT_PREPARE,
    VERDICT_FADE_IN,
    VERDICT_ROW,
    VERDICT_REVEAL,
    VERDICT_NEXT_ROW,
    VERDICT_CONFIRM,
    VERDICT_FADE_OUT,
    VERDICT_DONE,
} VERDICT_PHASE;

typedef struct {
    const ENDING_ASSETS *assets;
    const TH02_SESSION *session;
    ENDING_STORY route;
    uint16_t style;
    WAIT wait;
    ENDING_REVEAL reveal;
    VERDICT_PHASE phase;
    uint8_t row;
} VERDICT;

// A writable view of verdict.c's private state, for tests.
extern VERDICT *const p_Verdict;
