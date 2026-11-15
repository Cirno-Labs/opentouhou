#include "th07/screen_effect.h"

#include "gfx_recorder.h"
#include "test.h"
#include "th07_scene_stubs.h"

#include "th07/anm.h"
#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/stubs.h"

#include "core/base/color.h"
#include "core/base/memory.h"
#include "core/gfx/private.h"

#include "stg/chain.h"
#include "stg/rng.h"

static void M_Setup(void)
{
    Chain_Release(&g_Chain);
    Chain_Init(&g_Chain);
    SceneStubCalls_Reset();
    g_GameManager.run.stage_frames = 100;
    g_GameSpeed = 1.0f;
    Rng_SetSeed(&g_Rng, 1234);
    GfxRecorder_Start();
}

static void M_Teardown(void)
{
    Chain_Release(&g_Chain);
    Chain_Init(&g_Chain);
    GfxRecorder_Stop();
}

// Draws the effects and returns the alpha of quad index, or -1 for none.
static int M_DrawnAlpha(const int32_t index)
{
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    if (GfxRecorder_QuadCount() <= index) {
        return -1;
    }
    return GfxRecorder_Quad(index)->vertices[0].color.alpha;
}

// Returns the top left corner of a 64 x 16 sprite drawn with it at 0, 0.
static VEC3 M_SpriteCorner(void)
{
    Anm_Init(&g_HeapAllocator, &g_Rng);
    CHECK(Result_Log(
        Anm_CreateTexture(0, 256, 64, ANM_TEXTURE_RGBA8), LOG_LEVEL_ERROR));
    Anm_SetSpriteEntry(
        3,
        &(ANM_SPRITE) { .x1 = 64.0f,
                        .y1 = 16.0f,
                        .texture_width = 256.0f,
                        .texture_height = 64.0f,
                        .u1 = 0.25f,
                        .v1 = 0.25f,
                        .width = 64.0f,
                        .height = 16.0f,
                        .scale_x = 1.0f,
                        .scale_y = 1.0f });
    ANM_VM vm;
    Anm_CreateVm(&vm);
    Anm_InitVm(&vm);
    Anm_SetSprite(&vm, 3);
    vm.pos = (VEC3) { 32.0f, 8.0f, 0.0f };
    const ANM_DRAW draw = Anm_ScreenDraw();
    Gfx_BeginTick((GFX_COLOR) {});
    Anm_DrawNoRound(&vm, &draw);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 1);
    const VEC3 corner = GfxRecorder_Quad(0)->vertices[0].position;
    Anm_Shutdown();
    return corner;
}

static float M_ExpectedAxis(RNG *const rng, const float shake)
{
    switch (Rng_U32InRange(rng, 3)) {
    case 1:
        return shake;
    case 2:
        return -shake;
    default:
        return 0.0f;
    }
}

TEST(a_fade_ends_after_its_duration_and_removes_its_draw_element)
{
    M_Setup();
    ScreenEffect_FadeIn(4, 0xffffff);
    CHECK(g_Chain.draw.next != nullptr);
    CHECK_EQ_INT(g_Chain.calc.next->priority, 15);
    CHECK_EQ_INT(g_Chain.draw.next->priority, 17);
    for (int i = 0; i < 4; i++) {
        Chain_RunCalc(&g_Chain);
        CHECK(g_Chain.calc.next != nullptr);
    }
    Chain_RunCalc(&g_Chain);
    CHECK(g_Chain.calc.next == nullptr);
    CHECK(g_Chain.draw.next == nullptr);
    M_Teardown();
}

TEST(a_fade_runs_at_the_game_speed)
{
    M_Setup();
    g_GameSpeed = 0.5f;
    ScreenEffect_FadeOutScreen(2, 0);
    for (int i = 0; i < 4; i++) {
        Chain_RunCalc(&g_Chain);
    }
    // 1.5 of 2 frames.
    CHECK_EQ_INT(M_DrawnAlpha(0), 191);
    Chain_RunCalc(&g_Chain);
    CHECK(g_Chain.calc.next == nullptr);
    M_Teardown();
}

TEST(fades_record_the_original_rectangles_and_preceding_calc_alpha)
{
    M_Setup();
    ScreenEffect_FadeIn(4, 0x123456);
    ScreenEffect_FadeOut(4, 0xabcdef);
    ScreenEffect_FadeOutScreen(4, 0x654321);
    Chain_RunCalc(&g_Chain);
    Chain_RunCalc(&g_Chain);
    const RNG before = g_Rng;
    Gfx_BeginTick((GFX_COLOR) {});
    Chain_RunDraw(&g_Chain);
    Gfx_EndTick();
    CHECK_EQ_INT(GfxRecorder_QuadCount(), 3);
    const GFX_VERTEX *const screen = GfxRecorder_Quad(0)->vertices;
    const GFX_VERTEX *const play_area = GfxRecorder_Quad(1)->vertices;
    const GFX_VERTEX *const screen_out = GfxRecorder_Quad(2)->vertices;
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
    CHECK(screen_out[0].position.x == 0.0f);
    CHECK(screen_out[2].position.x == 640.0f);
    CHECK(screen_out[2].position.y == 480.0f);
    CHECK_EQ_INT(screen_out[0].color.red, 0x65);
    CHECK_EQ_INT(screen_out[0].color.alpha, 63);
    CHECK(GfxRecorder_Quad(0)->state.blend == GFX_BLEND_ALPHA);
    CHECK(GfxRecorder_Quad(0)->texture == nullptr);
    CHECK_EQ_INT(g_Rng.seed, before.seed);
    M_Teardown();
}

TEST(a_flash_fades_the_play_area_from_its_alpha)
{
    M_Setup();
    ScreenEffect_Flash(4, 1, COLOR_ARGB(0x80, 0xcf, 0xcf, 0xff));
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(M_DrawnAlpha(0), 0x80);
    const GFX_VERTEX *const quad = GfxRecorder_Quad(0)->vertices;
    CHECK(quad[0].position.x == 32.0f);
    CHECK(quad[0].position.y == 16.0f);
    CHECK(quad[2].position.x == 416.0f);
    CHECK(quad[2].position.y == 464.0f);
    CHECK_EQ_INT(quad[0].color.red, 0xcf);
    CHECK_EQ_INT(quad[0].color.blue, 0xff);
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(M_DrawnAlpha(0), 96);
    Chain_RunCalc(&g_Chain);
    Chain_RunCalc(&g_Chain);
    CHECK_EQ_INT(M_DrawnAlpha(0), 32);
    Chain_RunCalc(&g_Chain);
    CHECK(g_Chain.calc.next == nullptr);
    M_Teardown();
}

TEST(a_flash_truncates_the_faded_part_before_subtracting)
{
    M_Setup();
    ScreenEffect_Flash(3, 1, COLOR_ARGB(0x50, 0xff, 0xff, 0xff));
    Chain_RunCalc(&g_Chain);
    Chain_RunCalc(&g_Chain);
    // 80 - trunc(26.67), not trunc(80 - 26.67).
    CHECK_EQ_INT(M_DrawnAlpha(0), 54);
    M_Teardown();
}

TEST(each_flash_after_the_first_starts_a_frame_in)
{
    M_Setup();
    ScreenEffect_Flash(2, 3, COLOR_ARGB(0x80, 0xff, 0xff, 0xff));
    const int expected[6] = { 128, 64, 0, 64, 0, 64 };
    for (int i = 0; i < 6; i++) {
        Chain_RunCalc(&g_Chain);
        CHECK_EQ_INT(M_DrawnAlpha(0), expected[i]);
    }
    Chain_RunCalc(&g_Chain);
    CHECK(g_Chain.calc.next == nullptr);
    M_Teardown();
}

TEST(a_shake_has_no_draw_element)
{
    M_Setup();
    ScreenEffect_Shake(3, 8, 0);
    CHECK(g_Chain.calc.next != nullptr);
    CHECK(g_Chain.draw.next == nullptr);
    M_Teardown();
}

// Runs from several seeds, so that some frames shake on each axis.
TEST(a_shake_moves_every_sprite_as_the_rng_picks)
{
    M_Setup();
    int moved = 0;
    for (uint16_t seed = 0; seed < 8; seed++) {
        Rng_SetSeed(&g_Rng, seed);
        RNG expected = g_Rng;
        ScreenEffect_Shake(3, 0, 5);
        for (int frame = 1; frame < 3; frame++) {
            Chain_RunCalc(&g_Chain);
            // At frame 1, 1.6666666 rather than the 1.6666667 of 5 * (1 / 3).
            const float shake = (float)(5 - 0) * (float)frame / (float)3;
            const float x = M_ExpectedAxis(&expected, shake);
            const float y = M_ExpectedAxis(&expected, shake);
            moved += x != 0.0f && y != 0.0f;
            const RNG drawn = g_Rng;
            const VEC3 corner = M_SpriteCorner();
            g_Rng = drawn;
            CHECK(corner.x == x + 0.5f);
            CHECK(corner.y == y + 0.5f);
        }
        CHECK(g_Rng.seed == expected.seed);
        Chain_RunCalc(&g_Chain);
        CHECK(g_Chain.calc.next == nullptr);
    }
    CHECK(moved > 0);
    Anm_SetShake(0.0f, 0.0f);
    M_Teardown();
}

TEST(a_shake_ends_and_leaves_its_last_offset)
{
    M_Setup();
    int moved = 0;
    for (uint16_t seed = 0; seed < 8; seed++) {
        Rng_SetSeed(&g_Rng, seed);
        Anm_SetShake(0.0f, 0.0f);
        ScreenEffect_Shake(3, 8, 0);
        Chain_RunCalc(&g_Chain);
        Chain_RunCalc(&g_Chain);
        const RNG drawn = g_Rng;
        const VEC3 last = M_SpriteCorner();
        g_Rng = drawn;
        moved += last.x != 0.5f || last.y != 0.5f;
        Chain_RunCalc(&g_Chain);
        CHECK(g_Chain.calc.next == nullptr);
        CHECK(g_Rng.seed == drawn.seed);
        const VEC3 after = M_SpriteCorner();
        CHECK(after.x == last.x);
        CHECK(after.y == last.y);
    }
    CHECK(moved > 0);
    Anm_SetShake(0.0f, 0.0f);
    M_Teardown();
}

TEST(a_shake_ends_at_once_early_in_a_stage)
{
    M_Setup();
    g_GameManager.run.stage_frames = 1;
    const RNG before = g_Rng;
    ScreenEffect_Shake(30, 8, 0);
    Chain_RunCalc(&g_Chain);
    CHECK(g_Chain.calc.next == nullptr);
    CHECK(g_Rng.seed == before.seed);
    M_Teardown();
}

TEST(releasing_the_chain_frees_every_effect)
{
    M_Setup();
    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < 32; i++) {
            ScreenEffect_Flash(60, 1, 0);
        }
        Chain_Release(&g_Chain);
        Chain_Init(&g_Chain);
    }
    CHECK(g_Chain.calc.next == nullptr);
    M_Teardown();
}
