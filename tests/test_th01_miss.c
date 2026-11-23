#include "th01/timer.h"
#include "th01/hud.h"
#include "th01/miss.h"
#include "th01/shots.h"
#include "th01/player.h"
#include "th01/pellets.h"
#include "th01/orb.h"
#include "th01/rng.h"

#include <string.h>

#include "pc98/zun/master_rng.h"

#include "core/base/arena.h"

#include "th01_video.h"
#include "th01_world.h"
#include "test.h"

typedef struct {
    ARENA arena;
    PTN miko, stage;
    GAME_TEXT_FONT font;
} CASE;

static void M_Init(CASE *const state, const int16_t left, const int16_t bombs)
{
    *state = (CASE) {};
    Th01Test_ResetWorld();
    memset(&g_Player, 0, sizeof(g_Player));
    g_Player.left = left;
    g_Player.remaining_bombs = bombs;
    g_Player.mode = PLAYER_MODE_SPECIAL_FIRST;
    g_Player.is_hit = true;
    g_Player.invincible = true;
    g_Player.invincibility_time = 71;
    g_Player.bomb_frame = 941;
    memset(&g_Orb, 0, sizeof(g_Orb));
    g_Orb.left = 100;
    g_Orb.top = 100;
    g_Orb.rotation_frame = 8;
    g_Rng = 1;
    GameInput_Init();
    g_GameInput.bomb = true;
    g_GameInput.shot = true;
    g_GameInput.strike = true;
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
    g_Shots.slots[0] = (SHOT) { .left = 120, .top = 200, .moving = true };
    Hud_Init(Arena_Allocator(&state->arena));
    Hud_Load(nullptr, &state->stage, &state->font);
    StageTimer_Init();
}

static MISS_CONTEXT M_Context(CASE *const state, const int16_t extra)
{
    return (MISS_CONTEXT) {
        .miko = &state->miko,
        .stage = &state->stage,
        .difficulty = SETTINGS_DIFFICULTY_NORMAL,
        .remaining_lives = 2,
        .extra_bombs = extra,
    };
}

TEST(miss_keeps_initial_artifacts_and_uses_one_random_draw)
{
    CASE state;
    M_Init(&state, 304, 1);
    memset(&g_Miss, 0, sizeof(g_Miss));
    Frame_Reset();
    const MISS_CONTEXT context = M_Context(&state, 1);
    CHECK(IS_OK(Miss_ValidateSprites(&state.miko, &state.stage)));
    uint32_t rng = g_Rng;
    const int32_t expected_image = MasterRng_Next(&rng) % 8 == 0 ? 23 : 22;
    Miss_Begin(&context);
    CHECK_EQ_INT(g_Rng, rng);
    CHECK_EQ_INT(Th01Test_Pixel(0, 304, 352), 6);
    CHECK_EQ_INT(Th01Test_Pixel(0, 304, 368), expected_image % 14 + 1);
    CHECK_EQ_INT(Th01Test_Pixel(0, 100, 100), 4);
    CHECK(!Miss_Resume(&context));
    CHECK_EQ_INT(g_StageTimer.time, 0);
    CHECK_EQ_INT(g_Player.mode, PLAYER_MODE_SPECIAL_FIRST);
    Frame_VSync();
    CHECK(!Miss_Resume(&context));
    CHECK_EQ_INT(g_Miss.phase, MISS_SHAKE_DELAY);
    CHECK_EQ_INT(g_Video.scroll_top, 0);
    CHECK_EQ_INT(g_VSync.count1, 1);
    Frame_VSync();
    CHECK(!Miss_Resume(&context));
    CHECK_EQ_INT(g_Miss.phase, MISS_SHAKE_WAIT);
    CHECK_EQ_INT(g_Miss.frame, 1);
    CHECK_EQ_INT(g_VSync.count1, 0);
    Frame_VSync();
    CHECK(!Miss_Resume(&context));
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
                MISS_CONTEXT context = M_Context(
                    &state,
                    variant == 0       ? 1
                        : variant == 1 ? 1
                                       : 2);
                context.difficulty = rank;
                context.remaining_lives = (int16_t)variant;
                const ORB orb = g_Orb;
                const size_t used = state.arena.used;
                Frame_Reset();
                g_VSync.count1 = counts[variant];
                g_VSync.count2 = 32760;
                memset(&g_Miss, 0, sizeof(g_Miss));
                Miss_Begin(&context);
                const uint32_t rng = g_Rng;
                int32_t edges = 0;
                while (!Miss_Resume(&context) && edges < 100) {
                    const MISS snapshot = g_Miss;
                    const FRAME_VSYNC timing = g_VSync;
                    CHECK(!Miss_Resume(&context));
                    CHECK(memcmp(&g_Miss, &snapshot, sizeof(MISS)) == 0);
                    CHECK(memcmp(&g_VSync, &timing, sizeof(FRAME_VSYNC)) == 0);
                    Frame_VSync();
                    edges++;
                }
                CHECK_EQ_INT(edges, variant == 0 ? 82 : variant == 1 ? 81 : 83);
                CHECK_EQ_INT(g_Rng, rng);
                CHECK_EQ_INT(state.arena.used, used);
                CHECK(memcmp(&g_Orb, &orb, sizeof(ORB)) == 0);
                const int16_t right =
                    starts[x] + 160 >= 608 ? 608 : starts[x] + 160;
                const int16_t left = starts[x] < 160 ? 0 : starts[x] - 160;
                CHECK_EQ_INT(g_Player.left, right + 20 * ((304 - right) / 20));
                CHECK_EQ_INT(
                    g_Miss.effect_left, left + 20 * ((304 - left) / 20));
                CHECK(g_Player.is_hit);
                CHECK(g_Player.invincible);
                CHECK_EQ_INT(g_Player.invincibility_time, 71);
                CHECK_EQ_INT(g_Player.bomb_frame, 941);
                CHECK(!g_GameInput.bomb);
                CHECK(g_GameInput.shot);
                CHECK(g_GameInput.strike);
                const int8_t deflection[] = { 15, 12, 10, 8 };
                CHECK_EQ_INT(g_Player.deflect_duration, deflection[rank]);
                CHECK_EQ_INT(g_Video.scroll_top, 0);
                CHECK_EQ_INT(g_Player.remaining_bombs, variant == 0 ? 2 : 5);
                CHECK_EQ_INT(g_StageTimer.time, 1000);
                CHECK_EQ_INT(g_PelletShared.speed, -2);
                CHECK_EQ_INT(g_VSync.count1, 0);
                CHECK(Miss_Resume(&context));
                Arena_Free(&state.arena);
            }
        }
    }
}
