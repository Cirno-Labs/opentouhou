#include "tests/test.h"
#include "tests/th02/th02_music_room_render.h"
#include "th02/clip.h"
#include "th02/grcg.h"
#include "th02/video.h"

#include "core/base/memory.h"
#include "pc98/zun/master_rng.h"

#include <string.h>

// The polygons draw into real pages, so each test sets those up first.
static void M_InitVideo(void)
{
    Video_Init(&g_HeapAllocator);
    Clip_Reset();
    Grcg_Init();
}

TEST(initialization_uses_ninety_six_rng_outputs_and_is_not_repeated)
{
    M_InitVideo();
    MUSIC_POLYGONS state = {};
    uint32_t seed = 1;
    uint32_t expected = 1;
    for (int call = 0; call < 96; call++) {
        MasterRng_Next(&expected);
    }
    TestMusicRoomRender_UpdateAndRenderPolygons(&state, &seed);
    CHECK(state.initialized);
    CHECK_EQ_INT(seed, expected);
    for (size_t i = 0; i < MUSIC_POLYGON_COUNT; i++) {
        CHECK(state.polygons[i].velocity_x != 0);
        CHECK(state.polygons[i].angular_velocity != 0);
        CHECK(state.polygons[i].velocity_y_subpixel >= 32);
        CHECK(state.polygons[i].velocity_y_subpixel <= 80);
    }
    const MUSIC_POLYGONS before = state;
    TestMusicRoomRender_UpdateAndRenderPolygons(&state, &seed);
    CHECK_EQ_INT(seed, expected);
    CHECK(memcmp(&before, &state, sizeof(state)) != 0);
}

TEST(motion_reflects_at_the_edges_and_wraps_words)
{
    M_InitVideo();
    MUSIC_POLYGONS state = { .initialized = true };
    state.polygons[0] =
        (MUSIC_POLYGON) { .x = 0, .y_subpixel = -1, .velocity_x = -1 };
    state.polygons[1] = (MUSIC_POLYGON) {
        .x = 639, .velocity_x = 1, .angle = 255, .angular_velocity = -3
    };
    state.polygons[2] = (MUSIC_POLYGON) { .x = 32767, .velocity_x = 1 };
    state.polygons[3].velocity_x = -32768;
    uint32_t seed = 1;
    TestMusicRoomRender_UpdateAndRenderPolygons(&state, &seed);
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

TEST(respawn_consumes_five_rng_outputs)
{
    M_InitVideo();
    MUSIC_POLYGONS state = { .initialized = true };
    state.polygons[0] = (MUSIC_POLYGON) { .x = 100,
                                          .y_subpixel = 7999,
                                          .velocity_y_subpixel = 1 };
    uint32_t seed = 7;
    uint32_t expected = 7;
    for (int call = 0; call < 5; call++) {
        MasterRng_Next(&expected);
    }
    TestMusicRoomRender_UpdateAndRenderPolygons(&state, &seed);
    CHECK_EQ_INT(state.polygons[0].y_subpixel, -1600);
    CHECK_EQ_INT(seed, expected);
    CHECK(state.polygons[0].velocity_x != 0);
    CHECK(state.polygons[0].angular_velocity != 0);
}
