#include "tests/test.h"
#include "tests/th01/th01_video.h"
#include "tests/th01/th01_world.h"
#include "th01/bomb.c"
#include "th01/bomb.h"
#include "th01/hud.h"
#include "th01/input.c"
#include "th01/orb_private.h"
#include "th01/pellets/private.h"
#include "th01/player_private.h"
#include "th01/rng.h"
#include "th01/shots.h"

#include "core/base/arena.h"
#include "pc98/zun/math16.h"

#include <string.h>

typedef struct {
    ARENA arena;
    PALETTE stage;
    PTN miko;
    BOMB_ASSETS assets;
} CASE;

static void M_Init(CASE *const state)
{
    *state = (CASE) {};
    Th01Test_ResetWorld();
    p_Pellets->slots[0].moving = true;
    VSyncIrq_Reset();
    Arena_Init(&state->arena, "bomb", 1024 * 1024);
    const ALLOCATOR *const allocator = Arena_Allocator(&state->arena);
    Video_Init(allocator);
    Hud_Init(allocator);
    Palette_Init(p_Palette);
    state->stage = g_Palette->saved;
    *p_StagePalette = state->stage;
    p_Player->left = 304;
    state->miko =
        (PTN) { .count = 4, .pixels = Arena_Alloc(&state->arena, 4 * 1024) };
    memset(state->miko.pixels, 15, 4 * 1024);
    for (int32_t i = 0; i < 2; i++) {
        const int32_t count = i == 0 ? 8 : 1;
        state->assets.kuji[i] =
            (GRC) { 32, 32, count, Arena_Alloc(&state->arena, count * 1024) };
        memset(state->assets.kuji[i].coverage, 1, count * 1024);
    }
    for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        g_Video->pages[1][i] = (uint8_t)(i % 16);
    }
}

TEST(full_bomb_preserves_waits_palette_events_and_frame_ownership)
{
    CASE state;
    M_Init(&state);
    CHECK(IS_OK(Bomb_ValidateAssets(&state.assets)));
    int32_t waits = 0;
    const size_t used = state.arena.used;
    for (int16_t frame = 0; frame <= 140; frame++) {
        p_Player->bomb_frame = (uint32_t)frame;
        p_Player->invincible = false;
        const uint64_t edge = g_VSync->edge;
        BOMB_UPDATE result =
            Bomb_BeginUpdate(&state.miko, &state.assets, frame);
        CHECK(p_Player->invincible);
        CHECK_EQ_INT(p_Player->bomb_frame, frame);
        const bool must_wait = frame == 140 || (frame > 50 && frame % 4 == 0);
        CHECK_EQ_INT(result == BOMB_UPDATE_WAIT, must_wait);
        CHECK_EQ_INT(g_VSync->edge, edge);
        if (must_wait) {
            waits++;
            const uint8_t before = g_Video->pages[0][184 * 640 + 304];
            const int16_t scroll = g_Video->scroll_top;
            CHECK_EQ_INT(Bomb_ResumeUpdate(), BOMB_UPDATE_WAIT);
            CHECK_EQ_INT(g_Video->pages[0][184 * 640 + 304], before);
            CHECK_EQ_INT(g_Video->scroll_top, scroll);
            VSyncIrq_Edge();
            result = Bomb_ResumeUpdate();
            const int16_t offsets[] = { 392, 0, 384, 0 };
            CHECK_EQ_INT(
                g_Video->scroll_top,
                frame == 140 ? 0 : offsets[frame % 16 / 4]);
            CHECK(VSyncIrq_DelayReady(1));
        }
        CHECK_EQ_INT(
            result, frame == 140 ? BOMB_UPDATE_DONE : BOMB_UPDATE_RUNNING);
        CHECK_EQ_INT(p_Player->bomb_damaging, frame >= 50 && frame < 140);
        if (frame == 2) {
            CHECK_EQ_INT(g_Palette->saved.colors[7][0], 14);
            CHECK_EQ_INT(g_Palette->saved.colors[7][1], 14);
            CHECK_EQ_INT(g_Palette->saved.colors[7][2], 15);
            CHECK_EQ_INT(m_Bomb.left[0], Math16_PolarX(304, 252, 2));
            CHECK_EQ_INT(m_Bomb.top[9], Math16_PolarY(184, 162, -2));
        }
        if (frame == 40) {
            CHECK_EQ_INT(m_Bomb.tint_offset, -9);
        }
    }
    CHECK_EQ_INT(waits, 23);
    CHECK_EQ_INT(state.arena.used, used);
    CHECK_EQ_INT(p_Pellets->slots[0].decay_frame, 1);
    CHECK_EQ_INT(p_Player->invincibility_time, 100);
    CHECK(memcmp(g_Video->pages[0], g_Video->pages[1], VIDEO_PAGE_SIZE) == 0);
    CHECK(memcmp(&g_Palette->saved, &state.stage, sizeof(PALETTE)) == 0);
    const int16_t previous_x = m_Bomb.left[0];
    const int16_t previous_y = m_Bomb.top[0];
    Bomb_BeginUpdate(&state.miko, &state.assets, 0);
    CHECK_EQ_INT(m_Bomb.left[0], previous_x);
    CHECK_EQ_INT(m_Bomb.top[0], previous_y);
    CHECK_EQ_INT(p_Pellets->slots[0].decay_frame, 1);
    Arena_Free(&state.arena);
}

TEST(damage_uses_full_unsigned_frame_and_last_life)
{
    memset(p_Player, 0, sizeof(*p_Player));
    p_Player->bomb_damaging = true;
    CHECK(Bomb_DealsDamage(0, 0));
    CHECK(Bomb_DealsDamage(0, 65550));
    CHECK(!Bomb_DealsDamage(0, 65536));
    CHECK(!Bomb_DealsDamage(1, 100));
    CHECK(!Bomb_DealsDamage(-1, 100));
    p_Player->bomb_damaging = false;
    CHECK(!Bomb_DealsDamage(0, 100));
}

TEST(square_inversion_rejects_edges_and_toggles_only_one_plane)
{
    CASE state;
    M_Init(&state);
    memset(g_Video->pages[0], 6, VIDEO_PAGE_SIZE);
    CHECK(!Video_Invert32(-1, 0, 1));
    CHECK(!Video_Invert32(0, -1, 1));
    CHECK(!Video_Invert32(608, 0, 1));
    CHECK(!Video_Invert32(0, 368, 1));
    CHECK(Video_Invert32(607, 367, 1));
    CHECK_EQ_INT(Th01Test_Pixel(0, 600, 367), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 631, 398), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 632, 398), 6);
    CHECK(Video_Invert32(607, 367, 2));
    CHECK_EQ_INT(Th01Test_Pixel(0, 600, 367), 5);
    CHECK_EQ_INT(Th01Test_Pixel(1, 600, 367), (367 * 640 + 600) % 16);
    Video_SetScroll(400);
    CHECK_EQ_INT(g_Video->scroll_top, 0);
    Video_SetScroll(-8);
    CHECK_EQ_INT(g_Video->scroll_top, 392);
    CHECK_EQ_INT(Th01Test_Pixel(0, 600, 367), 5);
    Arena_Free(&state.arena);
}

TEST(player_continuation_spans_real_bomb_scroll_wait)
{
    CASE state;
    M_Init(&state);
    p_Player->remaining_bombs = 2;
    p_Player->bomb_flag = PLAYER_BOMB_START;
    GameInput_Init();
    m_GameInput.shot = true;
    m_GameInput.strike = true;
    m_GameInput.bomb = true;
    memset(p_Orb, 0, sizeof(*p_Orb));
    p_Orb->left = 500;
    p_Orb->top = 100;
    Shots_Init();
    g_Rng = 1;
    const PLAYER_SPRITES sprites = { .miko = &state.miko };
    PLAYER_BOMB_CONTINUATION continuation = {};
    CHECK(Player_BeginUpdate(&continuation, &sprites));
    CHECK_EQ_INT(p_Player->remaining_bombs, 1);
    CHECK_EQ_INT(
        Bomb_BeginUpdate(&state.miko, &state.assets, 0), BOMB_UPDATE_RUNNING);
    Player_FinishBombUpdate(&continuation, &sprites, false);
    p_Player->bomb_frame = 64;
    m_GameInput.shot = m_GameInput.strike = true;
    CHECK(Player_BeginUpdate(&continuation, &sprites));
    const int8_t dash = p_Player->dash_cycle;
    CHECK_EQ_INT(
        Bomb_BeginUpdate(&state.miko, &state.assets, 64), BOMB_UPDATE_WAIT);
    CHECK_EQ_INT(Bomb_ResumeUpdate(), BOMB_UPDATE_WAIT);
    CHECK(continuation.pending);
    CHECK(g_GameInput->shot && g_GameInput->strike);
    VSyncIrq_Edge();
    CHECK_EQ_INT(Bomb_ResumeUpdate(), BOMB_UPDATE_RUNNING);
    Player_FinishBombUpdate(&continuation, &sprites, false);
    CHECK(!continuation.pending);
    CHECK_EQ_INT(p_Player->dash_cycle, dash);
    CHECK_EQ_INT(p_Player->bomb_frame, 64);
    CHECK(!g_GameInput->shot && !g_GameInput->strike);
    CHECK(VSyncIrq_DelayReady(1));
    Arena_Free(&state.arena);
}
