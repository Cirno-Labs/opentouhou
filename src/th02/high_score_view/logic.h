#pragma once

#include "th02/high_score_view/render.h"

typedef struct {
    uint16_t wait_frames;
    bool done;
} HIGH_SCORE_VIEW_EVENTS;

// render draws the screen: the real functions in production, a fake
// observer in tests.
void HighScoreView_Init(const HIGH_SCORE_VIEW_RENDER *render);

// Shows the scores for duration+1 frames and a 20-frame hold; null assets
// run it without drawing.
void HighScoreView_Begin(
    const HIGH_SCORES *scores, const TH02_SETTINGS *settings,
    const HIGH_SCORE_VIEW_ASSETS *assets, uint16_t duration);

// Runs one frame; after a clear poll, the next nonzero mask exits.
HIGH_SCORE_VIEW_EVENTS HighScoreView_Update(uint16_t keys);
