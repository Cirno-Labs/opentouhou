#pragma once
#include "th02/high_score_view/logic.h"
typedef struct {
    void (*begin)(void *context);
    void (*frame)(void *context, int16_t frame);
    void (*flip)(void *context, uint8_t drawing, uint8_t visible);
    void (*end)(void *context);
    void *context;
} TEST_HIGH_SCORE_VIEW;
void TestHighScoreView_Observe(const TEST_HIGH_SCORE_VIEW *view);
void TestHighScoreView_Begin(
    uint16_t duration, const TEST_HIGH_SCORE_VIEW *view);
