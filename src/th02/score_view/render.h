#pragma once

#include "th02/draw.h"
#include "th02/palette.h"
#include "th02/scores.h"
#include "th02/settings.h"
#include "th02/tram.h"

typedef struct {
    PATTERN logo[2];
    GFX_COLOR palette[16];
    TRAM_TEXT header;
    TRAM_TEXT shot_types[3];
} SCORE_VIEW_ASSETS;

void ScoreViewRender_Begin(
    const SCORES *scores, const TH02_SETTINGS *settings,
    const SCORE_VIEW_ASSETS *assets);
void ScoreViewRender_Frame(const SCORE_VIEW_ASSETS *assets, int16_t frame);
void ScoreViewRender_Flip(uint8_t drawing_page, uint8_t visible_page);
void ScoreViewRender_End(void);
