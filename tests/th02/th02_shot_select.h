#pragma once

#include "th02/shot_select/logic.h"

typedef enum {
    SHOT_SELECT_DESCRIPTION,
    SHOT_SELECT_BACKGROUND,
    SHOT_SELECT_PICTURE,
    SHOT_SELECT_DARKEN,
} TEST_SHOT_DRAW_KIND;

typedef struct {
    TEST_SHOT_DRAW_KIND kind;
    int8_t selection;
    int16_t left;
    int16_t top;
    uint8_t value;
} TEST_SHOT_DRAW;

typedef struct {
    TEST_SHOT_DRAW requests[32];
    size_t count;
} TEST_SHOT_TRACE;

void TestShotSelect_Begin(TH02_SESSION *session, TEST_SHOT_TRACE *trace);
