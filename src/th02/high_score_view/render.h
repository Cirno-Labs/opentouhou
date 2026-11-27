#pragma once

#include "th02/draw.h"
#include "th02/palette.h"
#include "th02/high_scores.h"
#include "th02/settings.h"
#include "th02/tram.h"

typedef struct {
    PATTERN logo[2];
    GFX_COLOR palette[16];
    TRAM_TEXT header;
    TRAM_TEXT shot_types[3];
} HIGH_SCORE_VIEW_ASSETS;

void HighScoreViewRender_Begin(
    const HIGH_SCORES *scores, const TH02_SETTINGS *settings,
    const HIGH_SCORE_VIEW_ASSETS *assets);
void HighScoreViewRender_Frame(
    const HIGH_SCORE_VIEW_ASSETS *assets, int16_t frame);
void HighScoreViewRender_Flip(uint8_t drawing_page, uint8_t visible_page);
void HighScoreViewRender_End(void);
