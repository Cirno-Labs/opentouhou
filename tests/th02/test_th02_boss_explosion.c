// The boss explosion's ring of dots, checked through the fake draw modules
// (tests/th02/fake_draw_circles.c) instead of pixels.

#include "tests/test.h"
#include "tests/th02/fake_draw.h"
#include "th02/boss/explosion.h"
#include "th02/grcg.h"
#include "th02/patterns.h"
#include "th02/sound_effects.h"

TEST(explosion_frame_draws_its_ring_of_dots_in_the_flash_color)
{
    SoundEffects_Init(nullptr);
    Grcg_Init();
    PATTERN_BANK patterns = { 0 };
    TestDraw_Reset();
    BossExplosion_Draw(&patterns, 0, 200, 200, 3);
    CHECK(g_TestDraw.count > 0);
    for (int32_t i = 0; i < g_TestDraw.count; i++) {
        const TEST_DRAW_CALL *const call = &g_TestDraw.calls[i];
        CHECK_EQ_INT(call->op, TEST_DRAW_DOT);
        // Frame 3: radius 216, so dots of size 216 / 64 + 1, in color 15.
        CHECK_EQ_INT(call->e, 4);
        CHECK_EQ_INT(call->color, 15);
        CHECK(call->a > 24 && call->a < 416);
    }
}
