#include "tests/test.h"
#include "th01/transition.c"

#include "core/base/arena.h"

#include <string.h>

TEST(zero_delay_finishes_and_supports_saved_palette_alias)
{
    ARENA arena;
    Arena_Init(&arena, "transition", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video->pages[0], 9, VIDEO_PAGE_SIZE);
    memset(g_Video->pages[1], 6, VIDEO_PAGE_SIZE);
    INTERLEAVE_ASSETS assets = {};
    PALETTE_STATE palette;
    Palette_Init(&palette);
    Palette_SetGray(&palette, 3);
    Transition_Init();
    CHECK(Transition_BeginDiagonal(
        &palette, &palette.saved, &palette.saved, &assets, 0, 123));
    CHECK(!m_Transition.active);
    CHECK_EQ_INT(m_Transition.frame, 130);
    CHECK(memcmp(g_Video->pages[0], g_Video->pages[1], VIDEO_PAGE_SIZE) == 0);
    CHECK_EQ_INT(palette.saved.colors[0][0], 3);
    CHECK_EQ_INT(g_Video->palette[0].red, 51);
    CHECK(Transition_Advance(
        &palette, &palette.saved, &palette.saved, &assets, 123));
    Arena_Free(&arena);
}

TEST(fractional_delays_include_the_final_empty_frame)
{
    ARENA arena;
    Arena_Init(&arena, "transition", 2 * 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video->pages[1], 7, VIDEO_PAGE_SIZE);
    INTERLEAVE_ASSETS assets = {};
    PALETTE_STATE palette;
    Palette_Init(&palette);
    PALETTE current = {};
    PALETTE target = {};
    target.colors[0][0] = 15;
    Transition_Init();
    CHECK(!Transition_BeginDiagonal(
        &palette, &current, &target, &assets, 40, 100));
    CHECK_EQ_INT(m_Transition.frame, 1);
    CHECK_EQ_INT(current.colors[0][0], 1);
    const PAGE_TRANSITION waiting = m_Transition;
    uint8_t *const frozen = Arena_Alloc(&arena, VIDEO_PAGE_SIZE * 2);
    memcpy(frozen, g_Video->pages[0], VIDEO_PAGE_SIZE * 2);
    CHECK(!Transition_Advance(&palette, &current, &target, &assets, 100));
    CHECK(memcmp(&m_Transition, &waiting, sizeof(waiting)) == 0);
    CHECK(memcmp(frozen, g_Video->pages[0], VIDEO_PAGE_SIZE * 2) == 0);
    CHECK(!Transition_Advance(&palette, &current, &target, &assets, 102));
    CHECK_EQ_INT(m_Transition.frame, 1);
    CHECK(!Transition_Advance(&palette, &current, &target, &assets, 103));
    CHECK_EQ_INT(m_Transition.frame, 2);
    CHECK_EQ_INT(m_Transition.credit, 30);
    CHECK(!Transition_Advance(&palette, &current, &target, &assets, 105));
    CHECK_EQ_INT(m_Transition.frame, 3);
    CHECK_EQ_INT(m_Transition.credit, 10);
    CHECK(!Transition_Advance(&palette, &current, &target, &assets, 408));
    CHECK_EQ_INT(m_Transition.frame, 129);
    CHECK(memcmp(g_Video->pages[0], g_Video->pages[1], VIDEO_PAGE_SIZE) == 0);
    CHECK(!Transition_Advance(&palette, &current, &target, &assets, 410));
    CHECK_EQ_INT(m_Transition.frame, 130);
    CHECK(!Transition_Advance(&palette, &current, &target, &assets, 411));
    CHECK(Transition_Advance(&palette, &current, &target, &assets, 412));
    CHECK(!m_Transition.active);
    CHECK_EQ_INT(current.colors[0][0], 15);
    Arena_Free(&arena);
}

TEST(live_target_mutation_and_large_tick_gap)
{
    ARENA arena;
    Arena_Init(&arena, "transition", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    INTERLEAVE_ASSETS assets = {};
    PALETTE_STATE palette;
    Palette_Init(&palette);
    PALETTE current = {};
    PALETTE target = {};
    target.colors[2][1] = 15;
    Transition_Init();
    CHECK(!Transition_BeginDiagonal(
        &palette, &current, &target, &assets, 65535, 0));
    CHECK_EQ_INT(current.colors[2][1], 1);
    target.colors[2][1] = 0;
    CHECK(Transition_Advance(&palette, &current, &target, &assets, UINT64_MAX));
    CHECK_EQ_INT(current.colors[2][1], 0);
    CHECK_EQ_INT(palette.saved.colors[2][1], 0);
    CHECK_EQ_INT(m_Transition.frame, 130);
    Arena_Free(&arena);
}

TEST(saved_target_stays_live_when_separate_signed_current_is_clamped)
{
    ARENA arena;
    Arena_Init(&arena, "transition", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    INTERLEAVE_ASSETS assets = {};
    PALETTE_STATE palette;
    Palette_Init(&palette);
    Palette_SetGray(&palette, 0);
    PALETTE current = {};
    current.colors[0][0] = 29;
    current.colors[1][0] = -29;
    Transition_Init();
    CHECK(Transition_BeginDiagonal(
        &palette, &current, &palette.saved, &assets, 0, 0));
    CHECK_EQ_INT(current.colors[0][0], 15);
    CHECK_EQ_INT(current.colors[1][0], -12);
    CHECK_EQ_INT(palette.saved.colors[0][0], 15);
    CHECK_EQ_INT(palette.saved.colors[1][0], 0);
    CHECK_EQ_INT(g_Video->palette[0].red, 255);
    CHECK_EQ_INT(g_Video->palette[1].red, 0);
    Arena_Free(&arena);
}

TEST(sub_tick_delays_catch_up_without_rounding_each_step)
{
    ARENA arena;
    Arena_Init(&arena, "transition", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    INTERLEAVE_ASSETS assets = {};
    PALETTE_STATE palette;
    Palette_Init(&palette);
    Transition_Init();
    CHECK(!Transition_BeginDiagonal(
        &palette, &palette.saved, &palette.saved, &assets, 1, 0));
    CHECK(!Transition_Advance(
        &palette, &palette.saved, &palette.saved, &assets, 1));
    CHECK_EQ_INT(m_Transition.frame, 17);
    CHECK_EQ_INT(m_Transition.credit, 2);
    CHECK(!Transition_Advance(
        &palette, &palette.saved, &palette.saved, &assets, 7));
    CHECK_EQ_INT(m_Transition.frame, 117);
    CHECK(Transition_Advance(
        &palette, &palette.saved, &palette.saved, &assets, 8));
    Arena_Free(&arena);
}
