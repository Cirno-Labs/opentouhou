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
    g_Extend.next = 1;
    g_Stage.score = 399999;
    Extend_UpdateAndRender();
    CHECK_EQ_INT(g_Session.lives, 1);
    CHECK_EQ_INT(g_Extend.next, 1);
    g_Stage.score = UINT32_C(0x80000000);
    Extend_UpdateAndRender();
    CHECK_EQ_INT(g_Session.lives, 2);
    CHECK_EQ_INT(g_Extend.resident_lives, 2);
    CHECK_EQ_INT(g_Extend.next, 2);
    CHECK_EQ_INT(g_PelletShared.speed, 1);
    Arena_Free(&arena);
}

TEST(cap_still_advances_next_and_threshold_product_wraps)
{
    Th01Test_ResetWorld();
    g_Session.lives = 6;
    g_Extend.next = -1;
    g_Stage.score = UINT32_MAX;
    Extend_UpdateAndRender();
    CHECK_EQ_INT(g_Extend.next, 0);
    CHECK_EQ_INT(g_Session.lives, 6);
    Extend_UpdateAndRender();
    CHECK_EQ_INT(g_Extend.next, 1);
    g_Extend.next = -1;
    g_Stage.score = 400000;
    Extend_UpdateAndRender();
    CHECK_EQ_INT(g_Extend.next, -1);
    g_Extend.next = 32767;
    g_Stage.score = UINT32_MAX;
    Extend_UpdateAndRender();
    CHECK_EQ_INT(g_Extend.next, -32768);
}

TEST(restart_uses_signed_resident_score_division)
{
    Th01Test_ResetWorld();
    CHECK_EQ_INT(Extend_NextForScore(0), 1);
    CHECK_EQ_INT(Extend_NextForScore(399999), 1);
    CHECK_EQ_INT(Extend_NextForScore(400000), 2);
    CHECK_EQ_INT(Extend_NextForScore(UINT32_C(0x7fffffff)), 5369);
    CHECK_EQ_INT(Extend_NextForScore(UINT32_C(0x80000000)), -5367);
    CHECK_EQ_INT(Extend_NextForScore(UINT32_MAX), 1);
}
