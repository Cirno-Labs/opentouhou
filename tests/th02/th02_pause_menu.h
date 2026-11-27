#pragma once

#include "th02/pause_menu.h"

typedef struct {
    PAUSE_MENU_DRAW_KIND kind;
    uint16_t x;
    uint16_t y;
    uint16_t attribute;
} TEST_PAUSE_DRAW;

typedef struct {
    TEST_PAUSE_DRAW draws[64];
    size_t count;
    int16_t tone;
} TEST_PAUSE_REQUESTS;

void TestPause_Observe(TEST_PAUSE_REQUESTS *requests);
