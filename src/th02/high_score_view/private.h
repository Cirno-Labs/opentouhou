#pragma once

// logic.c's own state; nothing outside the high_score_view/ directory
// sees this.

#include "th02/high_score_view/logic.h"

typedef enum {
    HIGH_SCORE_VIEW_POLL,
    HIGH_SCORE_VIEW_FLIP,
    HIGH_SCORE_VIEW_EXIT,
    HIGH_SCORE_VIEW_HOLD,
    HIGH_SCORE_VIEW_DONE,
} HIGH_SCORE_VIEW_PHASE;

typedef struct {
    const HIGH_SCORE_VIEW_ASSETS *assets;
    HIGH_SCORE_VIEW_PHASE phase;
    int16_t frame;
    uint16_t duration;
    uint8_t visible_page;
    bool input_allowed;
} HIGH_SCORE_VIEWER;

// Writable view of logic.c's private state, for tests.
extern HIGH_SCORE_VIEWER *const p_Viewer;
