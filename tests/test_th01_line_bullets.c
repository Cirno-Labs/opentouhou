#include "th01/line_bullets.h"

#include <string.h>

#include "th01_video.h"
#include "test.h"

TEST(endpoint_hit_test_uses_supplied_unclipped_coordinates)
{
    Video_Init(&g_HeapAllocator);
    const int16_t xs[] = { 300, 301, 331, 332 };
    const int16_t ys[] = { 384, 385, 400, 900 };
    for (int32_t x = 0; x < 4; x++) {
        for (int32_t y = 0; y < 4; y++) {
            memset(&g_Player, 0, sizeof(g_Player));
            g_Player.left = 300;
            g_Player.sliding = true;
            LineBullets_PutPatternedAndHitTest(100, 100, xs[x], ys[y], 7, 0);
            CHECK(g_Player.is_hit == (x > 0 && x < 3 && y > 0));
            g_Player.is_hit = false;
            g_Player.invincible = true;
            LineBullets_PutAndHitTest(100, 100, xs[x], ys[y], 7);
            CHECK(!g_Player.is_hit);
        }
    }
    memset(&g_Player, 0, sizeof(g_Player));
    g_Player.left = 300;
    LineBullets_PutAndHitTest(310, 390, 500, 390, 7);
    CHECK(!g_Player.is_hit);
    LineBullets_PutAndHitTest(310, 390, 310, 100, 7);
    CHECK(!g_Player.is_hit);
    LineBullets_Erase(100, 100, 310, 400);
    CHECK(!g_Player.is_hit);
    Mem_Free(g_Video.pages[0]);
}

TEST(pattern_is_horizontal_mask_and_axis_paths_differ)
{
    Video_Init(&g_HeapAllocator);
    Video_LinePatterned(3, 10, 3, 12, 5, 0xe0);
    for (int32_t y = 10; y <= 12; y++) {
        for (int32_t x = 0; x < 16; x++) {
            CHECK_EQ_INT(Th01Test_Pixel(0, x, y), x >= 3 && x <= 5 ? 5 : 0);
        }
    }
    Video_LinePatterned(7, 20, 7, 20, 6, 0x8080);
    CHECK_EQ_INT(Th01Test_Pixel(0, 7, 20), 6);
    CHECK_EQ_INT(Th01Test_Pixel(0, 15, 20), 6);
    Video_LinePatterned(10, 30, 12, 30, 7, 0);
    for (int32_t x = 10; x <= 12; x++) {
        CHECK_EQ_INT(Th01Test_Pixel(0, x, 30), 7);
    }
    Mem_Free(g_Video.pages[0]);
}

TEST(oblique_pattern_omits_final_cache_group)
{
    Video_Init(&g_HeapAllocator);
    Video_LinePatterned(0, 0, 2, 1, 4, 0xe0);
    for (int32_t x = 0; x < 8; x++) {
        CHECK_EQ_INT(Th01Test_Pixel(0, x, 0), x < 4 ? 4 : 0);
        CHECK_EQ_INT(Th01Test_Pixel(0, x, 1), 0);
    }
    Mem_Free(g_Video.pages[0]);
}

TEST(raw_restore_uses_neighbor_bytes_and_one_past_cursor)
{
    Video_Init(&g_HeapAllocator);
    memset(g_Video.pages[1], 7, VIDEO_PAGE_SIZE);
    LineBullets_Erase(16, 10, 32, 10);
    for (int32_t x = 0; x < 64; x++) {
        CHECK_EQ_INT(Th01Test_Pixel(0, x, 10), x >= 8 && x < 56 ? 7 : 0);
    }
    Video_Clear(0);
    LineBullets_Erase(16, 10, 16, 12);
    CHECK_EQ_INT(Th01Test_Pixel(0, 8, 10), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 8, 11), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 8, 12), 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 8, 13), 7);
    Video_Clear(0);
    LineBullets_Erase(16, 10, 16, 10);
    CHECK_EQ_INT(Th01Test_Pixel(0, 8, 9), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 39, 9), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 8, 10), 0);
    Mem_Free(g_Video.pages[0]);
}

TEST(segment_crossing_extension_restores_only_mapped_planes)
{
    Video_Init(&g_HeapAllocator);
    Video_Clear(1);
    memset(g_Video.pages[1], 15, VIDEO_PAGE_SIZE);
    Video_SelectDrawingPage(1);
    LineBullets_Erase(0, 1, 0, 1);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    for (int32_t x = 0; x < 24; x++) {
        CHECK_EQ_INT(Th01Test_Pixel(0, x, 0), 5);
    }
    CHECK_EQ_INT(Th01Test_Pixel(0, 24, 0), 1);
    Mem_Free(g_Video.pages[0]);
}

TEST(mask_writes_wrap_planar_addresses_and_keep_page_selection)
{
    Video_Init(&g_HeapAllocator);
    Video_SelectDrawingPage(1);
    Video_LinePatterned(0, 410, 1, 410, 6, 0);
    CHECK_EQ_INT(Th01Test_Pixel(1, 256, 0), 6);
    CHECK_EQ_INT(Th01Test_Pixel(1, 257, 0), 6);
    CHECK_EQ_INT(Th01Test_Pixel(0, 256, 0), 0);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    LineBullets_Erase(-5, -5, -1, -1);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    Mem_Free(g_Video.pages[0]);
}

TEST(raw_reset_coordinates_wrap_before_clipping)
{
    Video_Init(&g_HeapAllocator);
    memset(g_Video.pages[1], 15, VIDEO_PAGE_SIZE);
    Video_SelectDrawingPage(1);
    Video_LineErase(-11095, -16673, 15360, 23552);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], 0);
    }
    Video_LineErase(277, -32317, 16384, 16384);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 264, 0), 15);
    CHECK_EQ_INT(Th01Test_Pixel(0, 295, 0), 15);
    CHECK_EQ_INT(Th01Test_Pixel(0, 296, 0), 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 264, 1), 15);
    CHECK_EQ_INT(Th01Test_Pixel(0, 624, 398), 15);
    Mem_Free(g_Video.pages[0]);
}
