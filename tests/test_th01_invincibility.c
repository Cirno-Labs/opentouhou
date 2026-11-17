#include "th01/invincibility.h"

#include "th01/math.h"

#include "core/base/arena.h"

#include "test.h"

TEST(mask_spills_across_rows_and_keeps_rejected_selector)
{
    ARENA arena;
    Arena_Init(&arena, "invincibility", 1024 * 1024);
    VIDEO video;
    Video_Init(&video, Arena_Allocator(&arena));
    INVINCIBILITY_ASSETS assets = {};
    assets.masks[0][0] = 0xff;
    Video_SelectDrawingPage(&video, 1);
    Invincibility_Put(&video, &assets, -1, 100, 0);
    CHECK_EQ_INT(video.drawing_page, 1);
    Invincibility_Put(&video, &assets, 639, 100, 4);
    CHECK_EQ_INT(video.drawing_page, 1);
    Invincibility_Put(&video, &assets, 639, 100, 0);
    CHECK_EQ_INT(video.drawing_page, 0);
    CHECK_EQ_INT(video.pages[0][100 * 640 + 639], 10);
    for (int32_t x = 0; x < 7; x++) {
        CHECK_EQ_INT(video.pages[0][101 * 640 + x], 10);
    }
    CHECK_EQ_INT(video.pages[0][101 * 640 + 7], 0);
    Arena_Free(&arena);
}

TEST(sprite_restart_draws_three_random_values_per_new_slot)
{
    ARENA arena;
    Arena_Init(&arena, "invincibility", 1024 * 1024);
    VIDEO video;
    Video_Init(&video, Arena_Allocator(&arena));
    INVINCIBILITY state = {};
    INVINCIBILITY_ASSETS assets = {};
    PLAYER player = { .left = 300 };
    uint32_t rng = 41;
    Invincibility_UpdateAndRender(&state, &video, &assets, &player, &rng);
    CHECK_EQ_INT(state.frame[0], 255);
    const size_t used = arena.used;
    const INVINCIBILITY before = state;
    Invincibility_UpdateAndRender(&state, &video, &assets, &player, &rng);
    CHECK_EQ_INT(memcmp(&state, &before, sizeof(state)), 0);
    CHECK_EQ_INT(rng, 41);
    player.invincible = true;
    uint32_t expected = rng;
    INVINCIBILITY expected_state = {};
    for (int32_t i = 0; i < 8; i++) {
        expected_state.left[i] = (int16_t)(Math_Random(&expected) % 48 + 292);
        expected_state.top[i] = (int16_t)(Math_Random(&expected) % 48 + 352);
        expected_state.frame[i] = (int16_t)(Math_Random(&expected) % 7 + 2);
    }
    Invincibility_UpdateAndRender(&state, &video, &assets, &player, &rng);
    CHECK_EQ_INT(rng, expected);
    CHECK_EQ_INT(memcmp(&state, &expected_state, sizeof(state)), 0);
    CHECK_EQ_INT(arena.used, used);
    player.invincible = false;
    Invincibility_UpdateAndRender(&state, &video, &assets, &player, &rng);
    CHECK_EQ_INT(state.frame[0], 255);
    CHECK_EQ_INT(rng, expected);
    Arena_Free(&arena);
}

TEST(rect_restore_keeps_the_original_word_width)
{
    ARENA arena;
    Arena_Init(&arena, "invincibility", 1024 * 1024);
    VIDEO video;
    Video_Init(&video, Arena_Allocator(&arena));
    memset(video.pages[1], 7, VIDEO_PAGE_SIZE);
    INVINCIBILITY state = {};
    INVINCIBILITY_ASSETS assets = {};
    PLAYER player = {};
    uint32_t rng = 1;
    for (int32_t i = 0; i < 8; i++) {
        state.left[i] = 1;
        state.top[i] = 100;
    }
    Invincibility_UpdateAndRender(&state, &video, &assets, &player, &rng);
    CHECK_EQ_INT(video.pages[0][100 * 640 + 31], 7);
    CHECK_EQ_INT(video.pages[0][100 * 640 + 32], 0);
    CHECK_EQ_INT(rng, 1);
    Arena_Free(&arena);
}
