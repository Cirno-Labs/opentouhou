#include "th01/invincibility.h"
#include "th01/rng.h"

#include <string.h>

#include "pc98/zun/master_rng.h"

#include "core/base/arena.h"

#include "th01_sprites.h"
#include "test.h"

#include <string.h>

TEST(loads_the_last_four_cels_of_the_shape_sheet)
{
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    INVINCIBILITY_ASSETS assets;
    CHECK(!Result_Ignore(Invincibility_LoadAssets(vfs, &assets)));
    Vfs_Destroy(vfs);
    VFS *const sprites = Th01Test_Sprites();
    CHECK(IS_OK(Invincibility_LoadAssets(sprites, &assets)));
    const uint8_t first[8] = { 0x00, 0x00, 0x18, 0x2c, 0x2c, 0x18, 0x00, 0x00 };
    const uint8_t last[8] = { 0x18, 0x2c, 0x5a, 0xff, 0xff, 0x5a, 0x2c, 0x18 };
    CHECK(memcmp(assets.masks[0], first, 8) == 0);
    CHECK(memcmp(assets.masks[3], last, 8) == 0);
    Vfs_Destroy(sprites);
}

TEST(mask_spills_across_rows_and_keeps_rejected_selector)
{
    ARENA arena;
    Arena_Init(&arena, "invincibility", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    INVINCIBILITY_ASSETS assets = {};
    assets.masks[0][0] = 0xff;
    Video_SelectDrawingPage(1);
    Invincibility_Put(&assets, -1, 100, 0);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    Invincibility_Put(&assets, 639, 100, 4);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    Invincibility_Put(&assets, 639, 100, 0);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.pages[0][100 * 640 + 639], 10);
    for (int32_t x = 0; x < 7; x++) {
        CHECK_EQ_INT(g_Video.pages[0][101 * 640 + x], 10);
    }
    CHECK_EQ_INT(g_Video.pages[0][101 * 640 + 7], 0);
    Arena_Free(&arena);
}

TEST(sprite_restart_draws_three_random_values_per_new_slot)
{
    ARENA arena;
    Arena_Init(&arena, "invincibility", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    INVINCIBILITY state = {};
    INVINCIBILITY_ASSETS assets = {};
    memset(&g_Player, 0, sizeof(g_Player));
    g_Player.left = 300;
    g_Rng = 41;
    Invincibility_UpdateAndRender(&state, &assets);
    CHECK_EQ_INT(state.frame[0], 255);
    const size_t used = arena.used;
    const INVINCIBILITY before = state;
    Invincibility_UpdateAndRender(&state, &assets);
    CHECK_EQ_INT(memcmp(&state, &before, sizeof(state)), 0);
    CHECK_EQ_INT(g_Rng, 41);
    g_Player.invincible = true;
    uint32_t expected = g_Rng;
    INVINCIBILITY expected_state = {};
    for (int32_t i = 0; i < 8; i++) {
        expected_state.left[i] =
            (int16_t)(MasterRng_Next(&expected) % 48 + 292);
        expected_state.top[i] = (int16_t)(MasterRng_Next(&expected) % 48 + 352);
        expected_state.frame[i] = (int16_t)(MasterRng_Next(&expected) % 7 + 2);
    }
    Invincibility_UpdateAndRender(&state, &assets);
    CHECK_EQ_INT(g_Rng, expected);
    CHECK_EQ_INT(memcmp(&state, &expected_state, sizeof(state)), 0);
    CHECK_EQ_INT(arena.used, used);
    g_Player.invincible = false;
    Invincibility_UpdateAndRender(&state, &assets);
    CHECK_EQ_INT(state.frame[0], 255);
    CHECK_EQ_INT(g_Rng, expected);
    Arena_Free(&arena);
}

TEST(rect_restore_keeps_the_original_word_width)
{
    ARENA arena;
    Arena_Init(&arena, "invincibility", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video.pages[1], 7, VIDEO_PAGE_SIZE);
    INVINCIBILITY state = {};
    INVINCIBILITY_ASSETS assets = {};
    memset(&g_Player, 0, sizeof(g_Player));
    g_Rng = 1;
    for (int32_t i = 0; i < 8; i++) {
        state.left[i] = 1;
        state.top[i] = 100;
    }
    Invincibility_UpdateAndRender(&state, &assets);
    CHECK_EQ_INT(g_Video.pages[0][100 * 640 + 31], 7);
    CHECK_EQ_INT(g_Video.pages[0][100 * 640 + 32], 0);
    CHECK_EQ_INT(g_Rng, 1);
    Arena_Free(&arena);
}
