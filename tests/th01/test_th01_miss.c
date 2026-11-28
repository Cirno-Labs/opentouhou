#include "tests/test.h"
#include "tests/th01/th01_video.h"
#include "tests/th01/th01_world.h"
#include "th01/hud.h"
#include "th01/input.c"
#include "th01/miss.c"
#include "th01/miss.h"
#include "th01/orb.c"
#include "th01/orb.h"
#include "th01/pellets.h"
#include "th01/player.c"
#include "th01/player.h"
#include "th01/rng.h"
#include "th01/session.h"
#include "th01/shots.c"
#include "th01/shots.h"
#include "th01/timer.h"
#include "th01/vsync_irq.c"

#include "core/base/arena.h"
#include "pc98/zun/master_rng.h"

#include <string.h>

typedef struct {
    ARENA arena;
    PTN miko;
    PTN stage;
    GAME_TEXT_FONT font;
} CASE;

static void M_Init(CASE *const state, const int16_t left, const int16_t bombs)
{
    *state = (CASE) {};
    Th01Test_ResetWorld();
    memset(&m_Player, 0, sizeof(m_Player));
    m_Player.left = left;
    m_Player.remaining_bombs = bombs;
    m_Player.mode = PLAYER_MODE_SPECIAL_FIRST;
    m_Player.is_hit = true;
    m_Player.invincible = true;
    m_Player.invincibility_time = 71;
    m_Player.bomb_frame = 941;
    memset(&m_Orb, 0, sizeof(m_Orb));
    m_Orb.left = 100;
    m_Orb.top = 100;
    m_Orb.rotation_frame = 8;
    g_Rng = 1;
    GameInput_Init();
    m_GameInput.bomb = true;
    m_GameInput.shot = true;
    m_GameInput.strike = true;
    Arena_Init(&state->arena, "miss", 1024 * 1024);
    Video_Init(Arena_Allocator(&state->arena));
    memset(g_Video.pages[0], 6, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 2, VIDEO_PAGE_SIZE);
    state->miko =
        (PTN) { .count = 24, .pixels = Arena_Alloc(&state->arena, 24 * 1024) };
    state->stage =
        (PTN) { .count = 7, .pixels = Arena_Alloc(&state->arena, 7 * 1024) };
    for (int32_t i = 0; i < 24; i++) {
        memset(state->miko.pixels + i * 1024, i % 14 + 1, 1024);
    }
    memset(state->stage.pixels, 4, 7 * 1024);
    m_Shots.slots[0] = (SHOT) { .left = 120, .top = 200, .moving = true };
    Hud_Init(Arena_Allocator(&state->arena));
    Hud_Load(nullptr, &state->stage, &state->font);
    StageTimer_Init();
}

TEST(miss_keeps_initial_artifacts_and_uses_one_random_draw)
{
    CASE state;
    M_Init(&state, 304, 1);
    memset(&m_Miss, 0, sizeof(m_Miss));
    VSyncIrq_Reset();
    g_Session.settings.difficulty = SETTINGS_DIFFICULTY_NORMAL;
    g_Session.lives = 2;
    CHECK(IS_OK(Miss_ValidateSprites(&state.miko, &state.stage)));
    uint32_t rng = g_Rng;
    const int32_t expected_image = MasterRng_Next(&rng) % 8 == 0 ? 23 : 22;
    Miss_Begin(&state.miko, &state.stage);
    CHECK_EQ_INT(g_Rng, rng);
    CHECK_EQ_INT(Th01Test_Pixel(0, 304, 352), 6);
    CHECK_EQ_INT(Th01Test_Pixel(0, 304, 368), expected_image % 14 + 1);
    CHECK_EQ_INT(Th01Test_Pixel(0, 100, 100), 4);
    CHECK(!Miss_Resume(&state.miko, &state.stage, 1));
    CHECK_EQ_INT(g_Stage->time, 0);
    CHECK_EQ_INT(m_Player.mode, PLAYER_MODE_SPECIAL_FIRST);
    VSyncIrq_Edge();
    CHECK(!Miss_Resume(&state.miko, &state.stage, 1));
    CHECK_EQ_INT(m_Miss.phase, MISS_SHAKE_DELAY);
    CHECK_EQ_INT(g_Video.scroll_top, 0);
    CHECK_EQ_INT(g_VSync->count1, 1);
    VSyncIrq_Edge();
    CHECK(!Miss_Resume(&state.miko, &state.stage, 1));
    CHECK_EQ_INT(m_Miss.phase, MISS_SHAKE_WAIT);
    CHECK_EQ_INT(m_Miss.frame, 1);
    CHECK_EQ_INT(g_VSync->count1, 0);
    VSyncIrq_Edge();
    CHECK(!Miss_Resume(&state.miko, &state.stage, 1));
    CHECK_EQ_INT(g_Video.scroll_top, 392);
    Arena_Free(&state.arena);
}

TEST(full_miss_preserves_drift_state_timing_and_hud_cap_order)
{
    const int16_t starts[] = { 0, 1, 159, 304, 305, 448, 607, 608 };
    const int16_t counts[] = { 0, 7, -1 };
    for (int32_t x = 0; x < 8; x++) {
        for (int32_t rank = 0; rank < 4; rank++) {
            for (int32_t variant = 0; variant < 3; variant++) {
                CASE state;
                M_Init(&state, starts[x], variant == 0 ? 1 : 4);
                const int16_t extra_bombs = variant == 0 ? 1
                    : variant == 1                       ? 1
                                                         : 2;
                g_Session.settings.difficulty = rank;
                g_Session.lives = variant;
                const ORB orb = m_Orb;
                const size_t used = state.arena.used;
                VSyncIrq_Reset();
                m_VSync.count1 = counts[variant];
                m_VSync.count2 = 32760;
                memset(&m_Miss, 0, sizeof(m_Miss));
                Miss_Begin(&state.miko, &state.stage);
                const uint32_t rng = g_Rng;
                int32_t edges = 0;
                while (!Miss_Resume(&state.miko, &state.stage, extra_bombs)
                       && edges < 100) {
                    const MISS snapshot = m_Miss;
                    const VSYNC_COUNTERS timing = *g_VSync;
                    CHECK(!Miss_Resume(&state.miko, &state.stage, extra_bombs));
                    CHECK(memcmp(&m_Miss, &snapshot, sizeof(MISS)) == 0);
                    CHECK(memcmp(g_VSync, &timing, sizeof(timing)) == 0);
                    VSyncIrq_Edge();
                    edges++;
                }
                CHECK_EQ_INT(edges, variant == 0 ? 82 : variant == 1 ? 81 : 83);
                CHECK_EQ_INT(g_Rng, rng);
                CHECK_EQ_INT(state.arena.used, used);
                CHECK(memcmp(&m_Orb, &orb, sizeof(ORB)) == 0);
                const int16_t right =
                    starts[x] + 160 >= 608 ? 608 : starts[x] + 160;
                const int16_t left = starts[x] < 160 ? 0 : starts[x] - 160;
                CHECK_EQ_INT(m_Player.left, right + 20 * ((304 - right) / 20));
                CHECK_EQ_INT(
                    m_Miss.effect_left, left + 20 * ((304 - left) / 20));
                CHECK(m_Player.is_hit);
                CHECK(m_Player.invincible);
                CHECK_EQ_INT(m_Player.invincibility_time, 71);
                CHECK_EQ_INT(m_Player.bomb_frame, 941);
                CHECK(!g_GameInput->bomb);
                CHECK(g_GameInput->shot);
                CHECK(g_GameInput->strike);
                const int8_t deflection[] = { 15, 12, 10, 8 };
                CHECK_EQ_INT(m_Player.deflect_duration, deflection[rank]);
                CHECK_EQ_INT(g_Video.scroll_top, 0);
                CHECK_EQ_INT(m_Player.remaining_bombs, variant == 0 ? 2 : 5);
                CHECK_EQ_INT(g_Stage->time, 1000);
                CHECK_EQ_INT(g_Pellets->speed, -2);
                CHECK_EQ_INT(g_VSync->count1, 0);
                CHECK(Miss_Resume(&state.miko, &state.stage, extra_bombs));
                Arena_Free(&state.arena);
            }
        }
    }
}
