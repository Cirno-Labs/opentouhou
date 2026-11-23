#include "th01/miss.h"

#include "pc98/zun/master_rng.h"

#include "core/base/arena.h"

#include "test.h"

typedef struct {
    ARENA arena;
    VIDEO video;
    PLAYER player;
    GAME_INPUT input;
    ORB orb;
    SHOTS shots;
    uint32_t rng;
    PTN miko, stage;
    int32_t events[8], event_count;
    int16_t previous_lives, previous_bombs;
} CASE;

static void M_Event(CASE *const state, const int32_t kind)
{
    state->events[state->event_count++] = kind;
}

static void M_Decay(void *const user)
{
    CASE *const state = user;
    M_Event(state, 0);
    CHECK_EQ_INT(state->player.mode, PLAYER_MODE_SPECIAL_FIRST);
    CHECK_EQ_INT(state->shots.slots[0].moving, 0);
    CHECK_EQ_INT(Video_ReadPixel(&state->video, 0, 120, 200), 2);
    CHECK_EQ_INT(Video_ReadPixel(&state->video, 0, state->player.left, 352), 2);
}

static void M_Lives(void *const user, const int16_t previous)
{
    CASE *const state = user;
    M_Event(state, 1);
    state->previous_lives = previous;
    CHECK_EQ_INT(state->player.mode, PLAYER_MODE_REGULAR);
}

static void M_Bombs(void *const user, const int16_t previous)
{
    CASE *const state = user;
    M_Event(state, 2);
    state->previous_bombs = previous;
}

static void M_Timer(void *const user)
{
    M_Event(user, 3);
}

static void M_Lower(void *const user, const float minimum, const float delta)
{
    M_Event(user, 4);
    CHECK(minimum == 0.0f);
    CHECK(delta == -0.05f);
}

static void M_Init(CASE *const state, const int16_t left, const int16_t bombs)
{
    *state = (CASE) { .player = { .left = left,
                                  .remaining_bombs = bombs,
                                  .mode = PLAYER_MODE_SPECIAL_FIRST,
                                  .is_hit = true,
                                  .invincible = true,
                                  .invincibility_time = 71,
                                  .bomb_frame = 941 },
                      .input = { .bomb = true, .shot = true, .strike = true },
                      .orb = { .left = 100, .top = 100, .rotation_frame = 8 },
                      .rng = 1 };
    Arena_Init(&state->arena, "miss", 1024 * 1024);
    Video_Init(&state->video, Arena_Allocator(&state->arena));
    memset(state->video.pages[0], 6, VIDEO_PAGE_SIZE);
    memset(state->video.pages[1], 2, VIDEO_PAGE_SIZE);
    state->miko =
        (PTN) { .count = 24, .pixels = Arena_Alloc(&state->arena, 24 * 1024) };
    state->stage =
        (PTN) { .count = 7, .pixels = Arena_Alloc(&state->arena, 7 * 1024) };
    for (int32_t i = 0; i < 24; i++) {
        memset(state->miko.pixels + i * 1024, i % 14 + 1, 1024);
    }
    memset(state->stage.pixels, 4, 7 * 1024);
    state->shots.slots[0] = (SHOT) { .left = 120, .top = 200, .moving = true };
}

static MISS_CONTEXT M_Context(CASE *const state, const int16_t extra)
{
    return (MISS_CONTEXT) {
        .video = &state->video,
        .player = &state->player,
        .input = &state->input,
        .orb = &state->orb,
        .shots = &state->shots,
        .rng = &state->rng,
        .miko = &state->miko,
        .stage = &state->stage,
        .difficulty = SETTINGS_DIFFICULTY_NORMAL,
        .remaining_lives = 2,
        .extra_bombs = extra,
        .pellets_decay = M_Decay,
        .hud_lives = M_Lives,
        .hud_bombs = M_Bombs,
        .timer_extend = M_Timer,
        .pellet_speed_lower = M_Lower,
        .user = state,
    };
}

TEST(miss_keeps_initial_artifacts_and_uses_one_random_draw)
{
    CASE state;
    M_Init(&state, 304, 1);
    MISS miss = {};
    FRAME_VSYNC vsync = {};
    const MISS_CONTEXT context = M_Context(&state, 1);
    CHECK(IS_OK(Miss_ValidateSprites(&state.miko, &state.stage)));
    uint32_t rng = state.rng;
    const int32_t expected_image = MasterRng_Next(&rng) % 8 == 0 ? 23 : 22;
    Miss_Begin(&miss, &context, &vsync);
    CHECK_EQ_INT(state.rng, rng);
    CHECK_EQ_INT(Video_ReadPixel(&state.video, 0, 304, 352), 6);
    CHECK_EQ_INT(
        Video_ReadPixel(&state.video, 0, 304, 368), expected_image % 14 + 1);
    CHECK_EQ_INT(Video_ReadPixel(&state.video, 0, 100, 100), 4);
    CHECK(!Miss_Resume(&miss, &context, &vsync));
    CHECK_EQ_INT(state.event_count, 0);
    CHECK_EQ_INT(state.player.mode, PLAYER_MODE_SPECIAL_FIRST);
    Frame_VSync(&vsync);
    CHECK(!Miss_Resume(&miss, &context, &vsync));
    CHECK_EQ_INT(miss.phase, MISS_SHAKE_DELAY);
    CHECK_EQ_INT(state.video.scroll_top, 0);
    CHECK_EQ_INT(vsync.count1, 1);
    Frame_VSync(&vsync);
    CHECK(!Miss_Resume(&miss, &context, &vsync));
    CHECK_EQ_INT(miss.phase, MISS_SHAKE_WAIT);
    CHECK_EQ_INT(miss.frame, 1);
    CHECK_EQ_INT(vsync.count1, 0);
    Frame_VSync(&vsync);
    CHECK(!Miss_Resume(&miss, &context, &vsync));
    CHECK_EQ_INT(state.video.scroll_top, 392);
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
                const int16_t previous_bombs = state.player.remaining_bombs;
                const ORB orb = state.orb;
                const size_t used = state.arena.used;
                FRAME_VSYNC vsync = { .count1 = counts[variant],
                                      .count2 = 32760 };
                MISS miss = {};
                Miss_Begin(&miss, &context, &vsync);
                const uint32_t rng = state.rng;
                int32_t edges = 0;
                while (!Miss_Resume(&miss, &context, &vsync) && edges < 100) {
                    const MISS snapshot = miss;
                    const FRAME_VSYNC timing = vsync;
                    CHECK(!Miss_Resume(&miss, &context, &vsync));
                    CHECK(memcmp(&miss, &snapshot, sizeof(MISS)) == 0);
                    CHECK(memcmp(&vsync, &timing, sizeof(FRAME_VSYNC)) == 0);
                    Frame_VSync(&vsync);
                    edges++;
                }
                CHECK_EQ_INT(edges, variant == 0 ? 82 : variant == 1 ? 81 : 83);
                CHECK_EQ_INT(state.rng, rng);
                CHECK_EQ_INT(state.arena.used, used);
                CHECK(memcmp(&state.orb, &orb, sizeof(ORB)) == 0);
                const int16_t right =
                    starts[x] + 160 >= 608 ? 608 : starts[x] + 160;
                const int16_t left = starts[x] < 160 ? 0 : starts[x] - 160;
                CHECK_EQ_INT(
                    state.player.left, right + 20 * ((304 - right) / 20));
                CHECK_EQ_INT(miss.effect_left, left + 20 * ((304 - left) / 20));
                CHECK(state.player.is_hit);
                CHECK(state.player.invincible);
                CHECK_EQ_INT(state.player.invincibility_time, 71);
                CHECK_EQ_INT(state.player.bomb_frame, 941);
                CHECK(!state.input.bomb);
                CHECK(state.input.shot);
                CHECK(state.input.strike);
                const int8_t deflection[] = { 15, 12, 10, 8 };
                CHECK_EQ_INT(state.player.deflect_duration, deflection[rank]);
                CHECK_EQ_INT(state.video.scroll_top, 0);
                CHECK_EQ_INT(state.previous_lives, variant + 1);
                CHECK_EQ_INT(
                    state.player.remaining_bombs, variant == 0 ? 2 : 5);
                CHECK_EQ_INT(state.event_count, variant == 2 ? 4 : 5);
                CHECK_EQ_INT(state.events[0], 0);
                CHECK_EQ_INT(state.events[1], 1);
                if (variant != 2) {
                    CHECK_EQ_INT(state.events[2], 2);
                    CHECK_EQ_INT(state.previous_bombs, previous_bombs);
                }
                CHECK_EQ_INT(state.events[state.event_count - 2], 3);
                CHECK_EQ_INT(state.events[state.event_count - 1], 4);
                CHECK_EQ_INT(vsync.count1, 0);
                CHECK(Miss_Resume(&miss, &context, &vsync));
                Arena_Free(&state.arena);
            }
        }
    }
}
