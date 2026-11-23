#include "th01/shootout_lasers.h"

#include "th01_sprites.h"
#include "th01_video.h"
#include "th01_world.h"
#include "test.h"

#include <string.h>

TEST(asset_errors_do_not_publish)
{
    Th01Test_ResetWorld();
    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    SHOOTOUT_LASERS_ASSETS *assets = nullptr;
    CHECK(!Result_Ignore(ShootoutLasers_LoadAssets(vfs, nullptr, &assets)));
    CHECK(assets == nullptr);
    Vfs_Destroy(vfs);
}

TEST(loads_each_width_as_a_cel_of_eight_shifts)
{
    VFS *const vfs = Th01Test_Sprites();
    SHOOTOUT_LASERS_ASSETS *assets = nullptr;
    CHECK(IS_OK(ShootoutLasers_LoadAssets(vfs, &g_HeapAllocator, &assets)));
    CHECK(memcmp(assets->masks[0][0], (uint8_t[2]) { 0x80, 0x00 }, 2) == 0);
    CHECK(memcmp(assets->masks[0][7], (uint8_t[2]) { 0x01, 0x00 }, 2) == 0);
    CHECK(memcmp(assets->masks[7][0], (uint8_t[2]) { 0xff, 0x00 }, 2) == 0);
    CHECK(memcmp(assets->masks[7][7], (uint8_t[2]) { 0x01, 0xfe }, 2) == 0);
    Mem_Free(assets);
    Vfs_Destroy(vfs);
}

TEST(spawn_retains_cursor_and_live_spawn_is_noop)
{
    Th01Test_ResetWorld();
    SHOOTOUT_LASER laser = { .cursor_left = 123, .cursor_y = -456 };
    ShootoutLasers_Spawn(&laser, 100, 64, 100, 400, 53, 7, 20, 8);
    CHECK(laser.alive && laser.damaging);
    CHECK_EQ_INT(laser.origin_left, 25600);
    CHECK_EQ_INT(laser.start_y, 16384);
    CHECK_EQ_INT(laser.step_x, 0);
    CHECK_EQ_INT(laser.step_y, 256);
    CHECK_EQ_INT(laser.extend_speed, 6);
    CHECK_EQ_INT(laser.tail_speed, 6);
    CHECK_EQ_INT(laser.cursor_left, 123);
    CHECK_EQ_INT(laser.cursor_y, -456);
    CHECK_EQ_INT(laser.width_mask, 7);
    const SHOOTOUT_LASER before = laser;
    ShootoutLasers_Spawn(&laser, -1, -1, 0, 0, -8, 15, -1, 0);
    CHECK(memcmp(&laser, &before, sizeof(laser)) == 0);
    laser.alive = false;
    ShootoutLasers_Spawn(&laser, 32760, -32760, -32760, 32760, -15, 1, 1, 1);
    CHECK_EQ_INT(laser.step_x, 181);
    CHECK_EQ_INT(laser.step_y, -181);
    CHECK_EQ_INT(laser.extend_speed, -1);
}

TEST(final_cached_group_omits_drawing_and_hit)
{
    Th01Test_ResetWorld();
    Video_Init(&g_HeapAllocator);
    SHOOTOUT_LASERS_ASSETS assets;
    memset(&assets, 0xff, sizeof(assets));
    memset(&g_Player, 0, sizeof(g_Player));
    g_Player.left = 300;
    g_Player.sliding = true;
    SHOOTOUT_LASER laser = {};
    ShootoutLasers_Spawn(&laser, 304, 377, 640, 377, 64, 7, 10, 1);
    ShootoutLasers_Update(&laser, &assets);
    CHECK_EQ_INT(laser.length, 8);
    CHECK_EQ_INT(laser.cursor_left, 312 * 256);
    CHECK(!g_Player.is_hit);
    CHECK_EQ_INT(Th01Test_Pixel(0, 304, 377), 0);
    ShootoutLasers_Update(&laser, &assets);
    CHECK(g_Player.is_hit);
    CHECK_EQ_INT(Th01Test_Pixel(0, 304, 377), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 319, 377), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 320, 377), 0);
    Mem_Free(g_Video.pages[0]);
}

TEST(hit_uses_cached_left_and_y_even_with_empty_masks)
{
    Th01Test_ResetWorld();
    Video_Init(&g_HeapAllocator);
    SHOOTOUT_LASERS_ASSETS assets = {};
    const int16_t xs[] = { 303, 304, 323, 324 };
    const int16_t ys[] = { 376, 377, 400 };
    for (int32_t x = 0; x < 4; x++) {
        for (int32_t y = 0; y < 3; y++) {
            for (int32_t mode = 0; mode < 3; mode++) {
                memset(&g_Player, 0, sizeof(g_Player));
                g_Player.left = 300;
                g_Player.invincible = mode == 1;
                SHOOTOUT_LASER laser = {};
                ShootoutLasers_Spawn(
                    &laser, xs[x], ys[y], xs[x], ys[y] + 10, 16, 7, 10, 1);
                laser.damaging = mode != 2;
                ShootoutLasers_Update(&laser, &assets);
                CHECK(
                    g_Player.is_hit == (x > 0 && x < 3 && y > 0 && mode == 0));
            }
        }
    }
    Mem_Free(g_Video.pages[0]);
}

TEST(mask_bytes_and_shifts_use_loaded_order_and_current_page)
{
    Th01Test_ResetWorld();
    Video_Init(&g_HeapAllocator);
    Video_SelectDrawingPage(1);
    SHOOTOUT_LASERS_ASSETS assets = {};
    assets.masks[7][3][0] = 0x10;
    assets.masks[7][3][1] = 0x40;
    SHOOTOUT_LASER laser = {};
    memset(&g_Player, 0, sizeof(g_Player));
    ShootoutLasers_Spawn(&laser, 99, 100, 99, 200, 16, 5, 10, 8);
    ShootoutLasers_Update(&laser, &assets);
    CHECK_EQ_INT(Th01Test_Pixel(1, 99, 100), 5);
    CHECK_EQ_INT(Th01Test_Pixel(1, 105, 100), 5);
    CHECK_EQ_INT(Th01Test_Pixel(1, 99, 101), 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 99, 100), 0);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    Mem_Free(g_Video.pages[0]);
}

TEST(detach_adopts_stopped_cursor_and_dies_before_age_or_put)
{
    Th01Test_ResetWorld();
    Video_Init(&g_HeapAllocator);
    Video_SelectDrawingPage(1);
    memset(g_Video.pages[1], 3, VIDEO_PAGE_SIZE);
    SHOOTOUT_LASERS_ASSETS assets = {};
    memset(&g_Player, 0, sizeof(g_Player));
    SHOOTOUT_LASER laser = {};
    ShootoutLasers_Spawn(&laser, 638, 100, 640, 100, 128, 7, 0, 1);
    laser.length = 100;
    ShootoutLasers_Update(&laser, &assets);
    CHECK(!laser.alive);
    CHECK_EQ_INT(laser.start_left, 640 * 256);
    CHECK_EQ_INT(laser.age, 0);
    CHECK_EQ_INT(laser.length, 100);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 624, 100), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 0, 101), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 16, 101), 0);
    Mem_Free(g_Video.pages[0]);
}

TEST(pool_order_preserves_page_change_and_signed_history)
{
    Th01Test_ResetWorld();
    Video_Init(&g_HeapAllocator);
    Video_SelectDrawingPage(1);
    SHOOTOUT_LASERS_ASSETS assets;
    memset(&assets, 0xff, sizeof(assets));
    memset(&g_ShootoutLasers, 0, sizeof(g_ShootoutLasers));
    memset(&g_Player, 0, sizeof(g_Player));
    ShootoutLasers_Spawn(
        &g_ShootoutLasers.slots[0], 100, 100, 100, 200, 8, 7, 0, 1);
    ShootoutLasers_Spawn(
        &g_ShootoutLasers.slots[9], 200, 100, 200, 200, 16, 5, 10, 1);
    ShootoutLasers_UpdateAll(&assets);
    CHECK_EQ_INT(g_ShootoutLasers.slots[0].start_y, 101 * 256);
    CHECK_EQ_INT(g_ShootoutLasers.slots[0].length, 0);
    CHECK_EQ_INT(g_ShootoutLasers.slots[9].length, 2);
    CHECK_EQ_INT(Th01Test_Pixel(0, 200, 100), 5);
    CHECK_EQ_INT(Th01Test_Pixel(1, 200, 100), 0);
    g_ShootoutLasers.slots[9].age = INT16_MAX;
    g_ShootoutLasers.slots[9].detach_age = INT16_MAX;
    ShootoutLasers_Update(&g_ShootoutLasers.slots[9], &assets);
    CHECK_EQ_INT(g_ShootoutLasers.slots[9].age, INT16_MIN);
    const SHOOTOUT_LASER history = g_ShootoutLasers.slots[9];
    g_ShootoutLasers.slots[9].alive = false;
    const SHOOTOUT_LASER dead = g_ShootoutLasers.slots[9];
    ShootoutLasers_UpdateAll(&assets);
    CHECK(memcmp(&g_ShootoutLasers.slots[9], &dead, sizeof(dead)) == 0);
    CHECK_EQ_INT(g_ShootoutLasers.slots[9].cursor_y, history.cursor_y);
    Mem_Free(g_Video.pages[0]);
}

TEST(reset_passes_raw_low_words_and_preserves_prefix_history)
{
    Th01Test_ResetWorld();
    uint8_t *const expected = Mem_Alloc(VIDEO_PAGE_SIZE);
    Video_Init(&g_HeapAllocator);
    memset(g_Video.pages[1], 7, VIDEO_PAGE_SIZE);
    Video_LineErase(256, 256, 256, 256);
    memcpy(expected, g_Video.pages[0], VIDEO_PAGE_SIZE);
    Mem_Free(g_Video.pages[0]);
    Video_Init(&g_HeapAllocator);
    memset(g_Video.pages[1], 7, VIDEO_PAGE_SIZE);
    memset(&g_ShootoutLasers, 0, sizeof(g_ShootoutLasers));
    ShootoutLasers_Spawn(&g_ShootoutLasers.slots[0], 1, 1, 1, 2, 8, 7, 20, 1);
    ShootoutLasers_Spawn(
        &g_ShootoutLasers.slots[1], 10, 100, 10, 400, 8, 7, 20, 1);
    const SHOOTOUT_LASER history = g_ShootoutLasers.slots[0];
    ShootoutLasers_EraseAndReset(1);
    CHECK(!g_ShootoutLasers.slots[0].alive);
    CHECK(g_ShootoutLasers.slots[1].alive);
    g_ShootoutLasers.slots[0].alive = true;
    CHECK(memcmp(&g_ShootoutLasers.slots[0], &history, sizeof(history)) == 0);
    CHECK(memcmp(g_Video.pages[0], expected, VIDEO_PAGE_SIZE) == 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 248, 255), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 0, 0), 0);
    Mem_Free(g_Video.pages[0]);
    Mem_Free(expected);
}

TEST(negative_sample_counts_wrap_and_stop_on_cursor_change)
{
    Th01Test_ResetWorld();
    Video_Init(&g_HeapAllocator);
    SHOOTOUT_LASERS_ASSETS assets = {};
    memset(&g_Player, 0, sizeof(g_Player));
    SHOOTOUT_LASER laser = {};
    ShootoutLasers_Spawn(&laser, 638, 100, 640, 100, -8, 7, 20, 1);
    ShootoutLasers_Update(&laser, &assets);
    CHECK(laser.alive);
    CHECK_EQ_INT(laser.length, -1);
    CHECK_EQ_INT(laser.age, 1);
    CHECK_EQ_INT(laser.cursor_left, 640 * 256);
    laser.detach_age = 1;
    ShootoutLasers_Update(&laser, &assets);
    CHECK(!laser.alive);
    CHECK_EQ_INT(laser.start_left, 640 * 256);
    CHECK_EQ_INT(laser.age, 1);
    Mem_Free(g_Video.pages[0]);
}
