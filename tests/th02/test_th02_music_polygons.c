#include "th02/music_room/private.h"

#include "pc98/zun/master_rng.h"

#include "test.h"

typedef struct {
    MUSIC_POLYGONS *state;
    MATH16_VECTOR first[16];
    uint8_t counts[16];
    size_t calls;
    bool check_respawn;
} TRACE;

static void M_Draw(
    void *const context, const MATH16_VECTOR *const vertices,
    const uint8_t count)
{
    TRACE *const trace = context;
    CHECK(trace->calls < MUSIC_POLYGON_COUNT);
    CHECK_EQ_INT(count, trace->calls / 4 + 3);
    CHECK_EQ_INT(vertices[count].x, vertices[0].x);
    CHECK_EQ_INT(vertices[count].y, vertices[0].y);
    if (trace->check_respawn && trace->calls == 0) {
        CHECK_EQ_INT(trace->state->polygons[0].y_subpixel, -1600);
    }
    trace->first[trace->calls] = vertices[0];
    trace->counts[trace->calls++] = count;
}

TEST(initialization_uses_ninety_six_rng_outputs_and_is_not_repeated)
{
    MUSIC_POLYGONS state = {};
    uint32_t seed = 1;
    uint32_t expected = 1;
    for (int call = 0; call < 96; call++) {
        MasterRng_Next(&expected);
    }
    TRACE trace = { .state = &state };
    MusicPolygons_UpdateAndRender(&state, &seed, M_Draw, &trace);
    CHECK(state.initialized);
    CHECK_EQ_INT(trace.calls, 16);
    CHECK_EQ_INT(seed, expected);
    for (size_t i = 0; i < MUSIC_POLYGON_COUNT; i++) {
        CHECK(state.polygons[i].velocity_x != 0);
        CHECK(state.polygons[i].angular_velocity != 0);
        CHECK(state.polygons[i].velocity_y_subpixel >= 32);
        CHECK(state.polygons[i].velocity_y_subpixel <= 80);
    }
    const MUSIC_POLYGONS before = state;
    trace.calls = 0;
    MusicPolygons_UpdateAndRender(&state, &seed, M_Draw, &trace);
    CHECK_EQ_INT(seed, expected);
    CHECK_EQ_INT(trace.calls, 16);
    CHECK(memcmp(&before, &state, sizeof(state)) != 0);
}

TEST(vertices_use_old_positions_before_reflection_and_word_wrap)
{
    MUSIC_POLYGONS state = { .initialized = true };
    state.polygons[0] =
        (MUSIC_POLYGON) { .x = 0, .y_subpixel = -1, .velocity_x = -1 };
    state.polygons[1] = (MUSIC_POLYGON) {
        .x = 639, .velocity_x = 1, .angle = 255, .angular_velocity = -3
    };
    state.polygons[2] = (MUSIC_POLYGON) { .x = 32767, .velocity_x = 1 };
    state.polygons[3].velocity_x = -32768;
    TRACE trace = { .state = &state };
    uint32_t seed = 1;
    MusicPolygons_UpdateAndRender(&state, &seed, M_Draw, &trace);
    CHECK_EQ_INT(trace.first[0].x, 64);
    CHECK_EQ_INT(trace.first[0].y, -1);
    CHECK_EQ_INT(state.polygons[0].x, -1);
    CHECK_EQ_INT(state.polygons[0].velocity_x, 1);
    CHECK_EQ_INT(state.polygons[1].x, 640);
    CHECK_EQ_INT(state.polygons[1].velocity_x, -1);
    CHECK_EQ_INT(state.polygons[1].angle, 252);
    CHECK_EQ_INT(state.polygons[2].x, -32768);
    CHECK_EQ_INT(state.polygons[2].velocity_x, -1);
    CHECK_EQ_INT(state.polygons[3].velocity_x, -32768);
    CHECK_EQ_INT(seed, 1);
}

TEST(respawn_consumes_five_outputs_and_emits_the_old_offscreen_polygon)
{
    MUSIC_POLYGONS state = { .initialized = true };
    state.polygons[0] = (MUSIC_POLYGON) { .x = 100,
                                          .y_subpixel = 7999,
                                          .velocity_y_subpixel = 1 };
    TRACE trace = { .state = &state, .check_respawn = true };
    uint32_t seed = 7;
    uint32_t expected = 7;
    for (int call = 0; call < 5; call++) {
        MasterRng_Next(&expected);
    }
    MusicPolygons_UpdateAndRender(&state, &seed, M_Draw, &trace);
    CHECK_EQ_INT(trace.calls, 16);
    CHECK_EQ_INT(trace.first[0].x, 164);
    CHECK_EQ_INT(trace.first[0].y, 499);
    CHECK_EQ_INT(state.polygons[0].y_subpixel, -1600);
    CHECK_EQ_INT(seed, expected);
    CHECK(state.polygons[0].velocity_x != 0);
    CHECK(state.polygons[0].angular_velocity != 0);
}

TEST(omitting_drawing_preserves_identical_motion_and_rng_state)
{
    MUSIC_POLYGONS drawn = {};
    MUSIC_POLYGONS skipped = {};
    uint32_t drawn_seed = 0x12345678;
    uint32_t skipped_seed = drawn_seed;
    for (int frame = 0; frame < 300; frame++) {
        TRACE trace = { .state = &drawn };
        MusicPolygons_UpdateAndRender(&drawn, &drawn_seed, M_Draw, &trace);
        MusicPolygons_UpdateAndRender(
            &skipped, &skipped_seed, nullptr, nullptr);
        CHECK_EQ_INT(drawn_seed, skipped_seed);
        CHECK(memcmp(&drawn, &skipped, sizeof(drawn)) == 0);
    }
}
