#include "th01/extend.h"
#include "core/base/arena.h"
#include "th01_video.h"
#include "th01_world.h"
#include "test.h"

#include <string.h>

typedef struct {
    HUD_CONTEXT display;
    EXTEND_CONTEXT context;
    PTN sprites;
    int16_t lives, next;
    int8_t resident_lives;
    int32_t effects;
} FIXTURE;

static void M_Effect(void *const user, const int16_t number)
{
    FIXTURE *const f = user;
    CHECK_EQ_INT(number, 15);
    CHECK_EQ_INT(f->lives, 2);
    CHECK_EQ_INT(f->resident_lives, 2);
    CHECK_EQ_INT(f->next, 1);
    CHECK_EQ_INT(g_PelletShared.speed, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 144, 0), 3);
    CHECK_EQ_INT(Th01Test_Pixel(1, 144, 0), 3);
    f->effects++;
}

TEST(threshold_uses_unsigned_score_and_one_award_per_call)
{
    Th01Test_ResetWorld();
    ARENA arena;
    Arena_Init(&arena, "extend", 1024 * 1024);
    FIXTURE f = { .lives = 1, .next = 1 };
    const ALLOCATOR *const a = Arena_Allocator(&arena);
    Video_Init(a);
    Hud_Init(a);
    f.sprites = (PTN) { .count = 1, .pixels = Allocator_Alloc(a, 1024) };
    memset(f.sprites.pixels, 3, 1024);
    f.display =
        (HUD_CONTEXT) { .sprites = &f.sprites, .remaining_lives = &f.lives };
    f.context = (EXTEND_CONTEXT) { .display = &f.display,
                                   .resident_lives = &f.resident_lives,
                                   .next = &f.next,
                                   .effect = M_Effect,
                                   .user = &f };
    g_Stage.score = 399999;
    Extend_UpdateAndRender(&f.context);
    CHECK_EQ_INT(f.lives, 1);
    CHECK_EQ_INT(f.next, 1);
    CHECK_EQ_INT(f.effects, 0);
    g_Stage.score = UINT32_C(0x80000000);
    Extend_UpdateAndRender(&f.context);
    CHECK_EQ_INT(f.lives, 2);
    CHECK_EQ_INT(f.next, 2);
    CHECK_EQ_INT(f.effects, 1);
    CHECK_EQ_INT(g_PelletShared.speed, 1);
    Arena_Free(&arena);
}

TEST(cap_still_advances_next_and_threshold_product_wraps)
{
    Th01Test_ResetWorld();
    int16_t lives = 6, next = -1;
    memset(&g_Stage, 0, sizeof(g_Stage));
    g_Stage.score = UINT32_MAX;
    const HUD_CONTEXT display = { .remaining_lives = &lives };
    const EXTEND_CONTEXT context = { .display = &display, .next = &next };
    Extend_UpdateAndRender(&context);
    CHECK_EQ_INT(next, 0);
    CHECK_EQ_INT(lives, 6);
    Extend_UpdateAndRender(&context);
    CHECK_EQ_INT(next, 1);
    next = -1;
    g_Stage.score = 400000;
    Extend_UpdateAndRender(&context);
    CHECK_EQ_INT(next, -1);
    next = 32767;
    g_Stage.score = UINT32_MAX;
    Extend_UpdateAndRender(&context);
    CHECK_EQ_INT(next, -32768);
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
