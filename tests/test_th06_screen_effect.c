#include "th06/screen_effect.h"

#include "gfx_recorder.h"
#include "test.h"

#include "th06/game.h"
#include "th06/game_manager.h"

#include "core/gfx/private.h"

#include "stg/chain.h"
#include "stg/rng.h"

static void M_Setup(void)
{
    Chain_Release(&g_Chain);
    Chain_Init(&g_Chain);
    GameManager_Create(&g_GameManager);
    Rng_SetSeed(&g_Rng, 1234);
}

static bool M_PlayAreaIsAtRest(void)
{
    const PLAY_RECT *const area = &g_GameManager.play_area;
    return area->x == 32.0f && area->y == 16.0f && area->width == 384.0f
        && area->height == 448.0f;
}

TEST(a_fade_ends_after_its_duration_and_removes_its_draw_element)
{
    M_Setup();
    ScreenEffect_FadeIn(4, 0xffffff);
    CHECK(g_Chain.draw.next != nullptr);
    for (int i = 0; i < 4; i++) {
        Chain_RunCalc(&g_Chain);
        CHECK(g_Chain.calc.next != nullptr);
    }
    Chain_RunCalc(&g_Chain);
    CHECK(g_Chain.calc.next == nullptr);
    CHECK(g_Chain.draw.next == nullptr);
}

TEST(a_shake_has_no_draw_element)
{
    M_Setup();
    ScreenEffect_Shake(3, 8, 0);
    CHECK(g_Chain.calc.next != nullptr);
    CHECK(g_Chain.draw.next == nullptr);
}

TEST(a_shake_moves_the_edges_as_the_rng_picks)
{
    M_Setup();
    RNG expected = g_Rng;
    ScreenEffect_Shake(3, 8, 0);
    for (int frame = 1; frame < 3; frame++) {
        Chain_RunCalc(&g_Chain);
        const float shake = (float)frame * (float)(0 - 8) / (float)3 + (float)8;
        const uint32_t horizontal = Rng_U32InRange(&expected, 3);
        const uint32_t vertical = Rng_U32InRange(&expected, 3);
        const PLAY_RECT *const area = &g_GameManager.play_area;
        CHECK(area->x == (horizontal == 1 ? 32.0f + shake : 32.0f));
        CHECK(area->width == (horizontal == 0 ? 384.0f : 384.0f - shake));
        CHECK(area->y == (vertical == 1 ? 16.0f + shake : 16.0f));
        CHECK(area->height == (vertical == 0 ? 448.0f : 448.0f - shake));
    }
    CHECK(g_Rng.seed == expected.seed);
}

TEST(a_shake_ends_with_the_play_area_at_rest)
{
    M_Setup();
    ScreenEffect_Shake(3, 8, 0);
    Chain_RunCalc(&g_Chain);
    Chain_RunCalc(&g_Chain);
    Chain_RunCalc(&g_Chain);
    CHECK(g_Chain.calc.next == nullptr);
    CHECK(M_PlayAreaIsAtRest());
}

TEST(a_shake_holds_still_while_the_game_is_frozen)
{
    M_Setup();
    g_GameManager.mode.frozen = true;
    const RNG before = g_Rng;
    ScreenEffect_Shake(2, 8, 0);
    for (int i = 0; i < 5; i++) {
        Chain_RunCalc(&g_Chain);
    }
    CHECK(g_Chain.calc.next != nullptr);
    CHECK(M_PlayAreaIsAtRest());
    CHECK(g_Rng.seed == before.seed);
}

TEST(releasing_the_chain_frees_every_effect)
{
    M_Setup();
    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < 16; i++) {
            ScreenEffect_FadeOut(60, 0);
        }
        Chain_Release(&g_Chain);
        Chain_Init(&g_Chain);
    }
    CHECK(g_Chain.calc.next == nullptr);
}

TEST(fades_record_the_original_rectangles_and_preceding_calc_alpha)
{
    M_Setup();
    GfxRecorder_Start();
    ScreenEffect_FadeIn(4, 0x123456);
    ScreenEffect_FadeOut(4, 0xabcdef);
    Chain_RunCalc(&g_Chain);
    Chain_RunCalc(&g_Chain);
    Gfx_BeginTick((GFX_COLOR) {});
    const RNG before = g_Rng;
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 2);
    const GFX_VERTEX *const screen = GfxRecorder_Quad(0)->vertices;
    const GFX_VERTEX *const play_area = GfxRecorder_Quad(1)->vertices;
    CHECK(screen[0].position.x == 0.0f);
    CHECK(screen[0].position.y == 0.0f);
    CHECK(screen[2].position.x == 640.0f);
    CHECK(screen[2].position.y == 480.0f);
    CHECK_EQ_INT(screen[0].color.red, 0x12);
    CHECK_EQ_INT(screen[0].color.green, 0x34);
    CHECK_EQ_INT(screen[0].color.blue, 0x56);
    CHECK_EQ_INT(screen[0].color.alpha, 191);
    CHECK(play_area[0].position.x == 32.0f);
    CHECK(play_area[0].position.y == 16.0f);
    CHECK(play_area[2].position.x == 416.0f);
    CHECK(play_area[2].position.y == 464.0f);
    CHECK_EQ_INT(play_area[0].color.alpha, 63);
    CHECK(GfxRecorder_Quad(0)->state.blend == GFX_BLEND_ALPHA);
    CHECK(GfxRecorder_Quad(0)->state.depth_test == GFX_DEPTH_OFF);
    CHECK(!GfxRecorder_Quad(0)->state.depth_write);
    CHECK(GfxRecorder_Quad(0)->texture == nullptr);
    CHECK_EQ_INT(g_Rng.seed, before.seed);
    GfxRecorder_Stop();
    Chain_Release(&g_Chain);
}
