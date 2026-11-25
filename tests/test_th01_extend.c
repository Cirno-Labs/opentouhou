#include "th01/extend.h"
#include "th01/session.h"
#include "core/base/arena.h"
#include "th01_video.h"
#include "th01_world.h"
#include "test.h"

#include <string.h>

TEST(threshold_uses_unsigned_score_and_one_award_per_call)
{
    Th01Test_ResetWorld();
    ARENA arena;
    Arena_Init(&arena, "extend", 1024 * 1024);
    const ALLOCATOR *const a = Arena_Allocator(&arena);
    Video_Init(a);
    Hud_Init(a);
    PTN sprites = { .count = 1, .pixels = Allocator_Alloc(a, 1024) };
    memset(sprites.pixels, 3, 1024);
    Hud_Load(nullptr, &sprites, nullptr);
    g_Session.lives = 1;
    Extend_Reset();
    g_Stage.score = 399999;
    Extend_UpdateAndRender();
    CHECK_EQ_INT(g_Session.lives, 1);
    CHECK_EQ_INT(Extend_Next(), 1);
    g_Stage.score = UINT32_C(0x80000000);
    Extend_UpdateAndRender();
    CHECK_EQ_INT(g_Session.lives, 2);
    CHECK_EQ_INT(Extend_ResidentLives(), 2);
    CHECK_EQ_INT(Extend_Next(), 2);
    CHECK_EQ_INT(g_Pellets->speed, 1);
    Arena_Free(&arena);
}

TEST(step_caps_lives_but_still_advances_next)
{
    Th01Test_ResetWorld();
    g_Session.lives = 6;
    Extend_Reset();
    g_Stage.score = 400000;
    Extend_UpdateAndRender();
    CHECK_EQ_INT(Extend_Next(), 2);
    CHECK_EQ_INT(g_Session.lives, 6);
    CHECK_EQ_INT(Extend_ResidentLives(), 0);
}

TEST(step_wraps_the_threshold_product_and_the_counter)
{
    int16_t next = -1;
    CHECK(Extend_Step(&next, UINT32_MAX));
    CHECK_EQ_INT(next, 0);
    CHECK(Extend_Step(&next, UINT32_MAX));
    CHECK_EQ_INT(next, 1);
    next = -1;
    CHECK(!Extend_Step(&next, 400000));
    CHECK_EQ_INT(next, -1);
    next = 32767;
    CHECK(Extend_Step(&next, UINT32_MAX));
    CHECK_EQ_INT(next, -32768);
}

TEST(restart_uses_signed_resident_score_division)
{
    CHECK_EQ_INT(Extend_NextForScore(0), 1);
    CHECK_EQ_INT(Extend_NextForScore(399999), 1);
    CHECK_EQ_INT(Extend_NextForScore(400000), 2);
    CHECK_EQ_INT(Extend_NextForScore(UINT32_C(0x7fffffff)), 5369);
    CHECK_EQ_INT(Extend_NextForScore(UINT32_C(0x80000000)), -5367);
    CHECK_EQ_INT(Extend_NextForScore(UINT32_MAX), 1);
}
