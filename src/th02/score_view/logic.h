#pragma once

#include "th02/score_view/render.h"

typedef struct {
    uint16_t wait_frames;
    bool done;
} SCORE_VIEW_EVENTS;

void ScoreView_Init(void);

// Shows the scores for duration+1 frames and a 20-frame hold; null assets
// run it without drawing.
void ScoreView_Begin(
    const SCORES *scores, const TH02_SETTINGS *settings,
    const SCORE_VIEW_ASSETS *assets, uint16_t duration);

// Runs one frame; after a clear poll, the next nonzero mask exits.
SCORE_VIEW_EVENTS ScoreView_Update(uint16_t keys);
