#pragma once

#include "th02/player.h"
#include "th02/background.h"
#include "th02/background/private.h"
#include "th02/grcg.h"
#include "th02/video.h"

#include "core/base/arena.h"
#include "test.h"

#include "th02_clip_fixture.h"

typedef struct {
    ARENA arena;
    PATTERN_BANK patterns;
} TEST_PLAYER_GRAPHICS;

static inline void TestPlayerGraphics_Init(TEST_PLAYER_GRAPHICS *const graphics)
{
    memset(graphics, 0, sizeof(*graphics));
    memset(&p_Background->cache, 0, sizeof(p_Background->cache));
    Arena_Init(&graphics->arena, "player graphics test", 2 * 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&graphics->arena);
    Video_Init(allocator);
    memset(g_Video.pages[0], 0, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 0, VIDEO_PAGE_SIZE);
    Clip_Reset();
    Grcg_Init();
    PatternBank_Init(&graphics->patterns, allocator);
    uint8_t body[32 * 48];
    uint8_t option[16 * 16];
    for (uint16_t sprite = 0; sprite < 10; sprite++) {
        memset(body, sprite + 1, sizeof(body));
        CHECK(Result_Ignore(
            PatternBank_Put(&graphics->patterns, sprite, 32, 48, body, 0)));
    }
    uint8_t large[32 * 32];
    for (uint16_t sprite = 10; sprite < 48; sprite++) {
        memset(large, sprite % 15 + 1, sizeof(large));
        CHECK(Result_Ignore(
            PatternBank_Put(&graphics->patterns, sprite, 32, 32, large, 0)));
    }
    for (uint16_t sprite = 48; sprite < 128; sprite++) {
        memset(option, sprite % 15 + 1, sizeof(option));
        CHECK(Result_Ignore(
            PatternBank_Put(&graphics->patterns, sprite, 16, 16, option, 0)));
        CHECK(Result_Ignore(
            PatternBank_ConvertTiny(&graphics->patterns, sprite)));
    }
    for (uint16_t sprite = 72; sprite <= 74; sprite++) {
        memset(option, sprite - 61, sizeof(option));
        CHECK(Result_Ignore(
            PatternBank_Put(&graphics->patterns, sprite, 16, 16, option, 0)));
        CHECK(Result_Ignore(
            PatternBank_ConvertTiny(&graphics->patterns, sprite)));
    }
}

static inline void TestPlayerGraphics_Free(TEST_PLAYER_GRAPHICS *const graphics)
{
    Arena_Free(&graphics->arena);
}
