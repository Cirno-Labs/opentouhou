#include "th06/screen_effect.h"

#include "test.h"

#include "th06/chain.h"
#include "th06/game.h"
#include "th06/game_manager.h"

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
