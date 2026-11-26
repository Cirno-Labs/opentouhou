#pragma once

#include "th02/main_menu.h"

typedef struct {
    int8_t selections[32];
    uint16_t attributes[32];
    size_t count;
    size_t init;
    size_t clear;
    size_t restore;
} TEST_MENU_TRACE;

void TestMenu_Begin(
    TH02_SESSION *session, TEST_MENU_TRACE *trace, bool unlocked, uint8_t demo);
