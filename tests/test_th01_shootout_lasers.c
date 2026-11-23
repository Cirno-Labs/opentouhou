#include "th01/shootout_lasers.h"

#include "th01_video.h"
#include "test.h"

TEST(asset_errors_do_not_allocate_or_publish)
{
    READER reader;
    Reader_OpenBuffer(&reader, nullptr, 0, nullptr);
    SHOOTOUT_LASERS_ASSETS *assets = nullptr;
    CHECK(!Result_Ignore(
        ShootoutLasers_LoadAssets(&reader, nullptr, 0, nullptr, &assets)));
    CHECK(assets == nullptr);
    uint8_t *const data = Mem_AllocZ(REIIDEN_IMAGE_SIZE);
    uint8_t *const scratch = Mem_Alloc(REIIDEN_IMAGE_SIZE);
    Reader_OpenBuffer(&reader, data, REIIDEN_IMAGE_SIZE, nullptr);
    CHECK(!Result_Ignore(
        ShootoutLasers_LoadAssets(&reader, nullptr, 0, nullptr, &assets)));
    CHECK_EQ_INT(Reader_Tell(&reader), 0);
    CHECK(!Result_Ignore(ShootoutLasers_LoadAssets(
        &reader, scratch, REIIDEN_IMAGE_SIZE, nullptr, &assets)));
    CHECK(assets == nullptr);
    Mem_Free(scratch);
    Mem_Free(data);
}

TEST(spawn_retains_cursor_and_live_spawn_is_noop)
{
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
    VIDEO video;
    Video_Init(&video, &g_HeapAllocator);
    SHOOTOUT_LASERS_ASSETS assets;
    memset(&assets, 0xff, sizeof(assets));
    PLAYER player = { .left = 300, .sliding = true };
    SHOOTOUT_LASER laser = {};
    ShootoutLasers_Spawn(&laser, 304, 377, 640, 377, 64, 7, 10, 1);
    ShootoutLasers_Update(&laser, &video, &player, &assets);
    CHECK_EQ_INT(laser.length, 8);
    CHECK_EQ_INT(laser.cursor_left, 312 * 256);
    CHECK(!player.is_hit);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, 304, 377), 0);
    ShootoutLasers_Update(&laser, &video, &player, &assets);
    CHECK(player.is_hit);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, 304, 377), 7);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, 319, 377), 7);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, 320, 377), 0);
    Mem_Free(video.pages[0]);
}

TEST(hit_uses_cached_left_and_y_even_with_empty_masks)
{
    VIDEO video;
    Video_Init(&video, &g_HeapAllocator);
    SHOOTOUT_LASERS_ASSETS assets = {};
    const int16_t xs[] = { 303, 304, 323, 324 };
    const int16_t ys[] = { 376, 377, 400 };
    for (int32_t x = 0; x < 4; x++) {
        for (int32_t y = 0; y < 3; y++) {
            for (int32_t mode = 0; mode < 3; mode++) {
                PLAYER player = { .left = 300, .invincible = mode == 1 };
                SHOOTOUT_LASER laser = {};
                ShootoutLasers_Spawn(
                    &laser, xs[x], ys[y], xs[x], ys[y] + 10, 16, 7, 10, 1);
                laser.damaging = mode != 2;
                ShootoutLasers_Update(&laser, &video, &player, &assets);
                CHECK(player.is_hit == (x > 0 && x < 3 && y > 0 && mode == 0));
            }
        }
    }
    Mem_Free(video.pages[0]);
}

TEST(mask_bytes_and_shifts_use_loaded_order_and_current_page)
{
    VIDEO video;
    Video_Init(&video, &g_HeapAllocator);
    Video_SelectDrawingPage(&video, 1);
    SHOOTOUT_LASERS_ASSETS assets = {};
    assets.masks[7][3][0] = 0x10;
    assets.masks[7][3][1] = 0x40;
    SHOOTOUT_LASER laser = {};
    PLAYER player = {};
    ShootoutLasers_Spawn(&laser, 99, 100, 99, 200, 16, 5, 10, 8);
    ShootoutLasers_Update(&laser, &video, &player, &assets);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 1, 99, 100), 5);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 1, 105, 100), 5);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 1, 99, 101), 0);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, 99, 100), 0);
    CHECK_EQ_INT(video.drawing_page, 1);
    Mem_Free(video.pages[0]);
}

TEST(detach_adopts_stopped_cursor_and_dies_before_age_or_put)
{
    VIDEO video;
    Video_Init(&video, &g_HeapAllocator);
    Video_SelectDrawingPage(&video, 1);
    memset(video.pages[1], 3, VIDEO_PAGE_SIZE);
    SHOOTOUT_LASERS_ASSETS assets = {};
    PLAYER player = {};
    SHOOTOUT_LASER laser = {};
    ShootoutLasers_Spawn(&laser, 638, 100, 640, 100, 128, 7, 0, 1);
    laser.length = 100;
    ShootoutLasers_Update(&laser, &video, &player, &assets);
    CHECK(!laser.alive);
    CHECK_EQ_INT(laser.start_left, 640 * 256);
    CHECK_EQ_INT(laser.age, 0);
    CHECK_EQ_INT(laser.length, 100);
    CHECK_EQ_INT(video.drawing_page, 0);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, 624, 100), 3);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, 0, 101), 3);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, 16, 101), 0);
    Mem_Free(video.pages[0]);
}

TEST(pool_order_preserves_page_change_and_signed_history)
{
    VIDEO video;
    Video_Init(&video, &g_HeapAllocator);
    Video_SelectDrawingPage(&video, 1);
    SHOOTOUT_LASERS_ASSETS assets;
    memset(&assets, 0xff, sizeof(assets));
    SHOOTOUT_LASERS lasers = {};
    PLAYER player = {};
    ShootoutLasers_Spawn(&lasers.slots[0], 100, 100, 100, 200, 8, 7, 0, 1);
    ShootoutLasers_Spawn(&lasers.slots[9], 200, 100, 200, 200, 16, 5, 10, 1);
    ShootoutLasers_UpdateAll(&lasers, &video, &player, &assets);
    CHECK_EQ_INT(lasers.slots[0].start_y, 101 * 256);
    CHECK_EQ_INT(lasers.slots[0].length, 0);
    CHECK_EQ_INT(lasers.slots[9].length, 2);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 0, 200, 100), 5);
    CHECK_EQ_INT(Th01Test_Pixel(&video, 1, 200, 100), 0);
    lasers.slots[9].age = INT16_MAX;
    lasers.slots[9].detach_age = INT16_MAX;
    ShootoutLasers_Update(&lasers.slots[9], &video, &player, &assets);
    CHECK_EQ_INT(lasers.slots[9].age, INT16_MIN);
    const SHOOTOUT_LASER history = lasers.slots[9];
    lasers.slots[9].alive = false;
    const SHOOTOUT_LASER dead = lasers.slots[9];
    ShootoutLasers_UpdateAll(&lasers, &video, &player, &assets);
    CHECK(memcmp(&lasers.slots[9], &dead, sizeof(dead)) == 0);
    CHECK_EQ_INT(lasers.slots[9].cursor_y, history.cursor_y);
    Mem_Free(video.pages[0]);
}

TEST(reset_passes_raw_low_words_and_preserves_prefix_history)
{
    VIDEO actual, expected;
    Video_Init(&actual, &g_HeapAllocator);
    Video_Init(&expected, &g_HeapAllocator);
    memset(actual.pages[1], 7, VIDEO_PAGE_SIZE);
    memset(expected.pages[1], 7, VIDEO_PAGE_SIZE);
    SHOOTOUT_LASERS lasers = {};
    ShootoutLasers_Spawn(&lasers.slots[0], 1, 1, 1, 2, 8, 7, 20, 1);
    ShootoutLasers_Spawn(&lasers.slots[1], 10, 100, 10, 400, 8, 7, 20, 1);
    const SHOOTOUT_LASER history = lasers.slots[0];
    Video_LineErase(&expected, 256, 256, 256, 256);
    ShootoutLasers_EraseAndReset(&lasers, &actual, 1);
    CHECK(!lasers.slots[0].alive);
    CHECK(lasers.slots[1].alive);
    lasers.slots[0].alive = true;
    CHECK(memcmp(&lasers.slots[0], &history, sizeof(history)) == 0);
    CHECK(memcmp(actual.pages[0], expected.pages[0], VIDEO_PAGE_SIZE) == 0);
    CHECK_EQ_INT(Th01Test_Pixel(&actual, 0, 248, 255), 7);
    CHECK_EQ_INT(Th01Test_Pixel(&actual, 0, 0, 0), 0);
    Mem_Free(actual.pages[0]);
    Mem_Free(expected.pages[0]);
}

TEST(negative_sample_counts_wrap_and_stop_on_cursor_change)
{
    VIDEO video;
    Video_Init(&video, &g_HeapAllocator);
    SHOOTOUT_LASERS_ASSETS assets = {};
    PLAYER player = {};
    SHOOTOUT_LASER laser = {};
    ShootoutLasers_Spawn(&laser, 638, 100, 640, 100, -8, 7, 20, 1);
    ShootoutLasers_Update(&laser, &video, &player, &assets);
    CHECK(laser.alive);
    CHECK_EQ_INT(laser.length, -1);
    CHECK_EQ_INT(laser.age, 1);
    CHECK_EQ_INT(laser.cursor_left, 640 * 256);
    laser.detach_age = 1;
    ShootoutLasers_Update(&laser, &video, &player, &assets);
    CHECK(!laser.alive);
    CHECK_EQ_INT(laser.start_left, 640 * 256);
    CHECK_EQ_INT(laser.age, 1);
    Mem_Free(video.pages[0]);
}
