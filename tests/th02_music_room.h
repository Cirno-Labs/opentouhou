#pragma once
#include "th02/music_room/logic.h"
typedef struct {
    void (*begin)(void *context, uint8_t selection);
    void (*track)(void *context, uint8_t selection, uint8_t color);
    void (*comment)(void *context, uint8_t selection);
    void (*frame)(void *context);
    void (*flip)(void *context, uint8_t drawing, uint8_t visible);
    void (*end)(void *context);
    void *context;
} TEST_MUSIC_VIEW;
void TestMusicRoom_Observe(const TEST_MUSIC_VIEW *view);
