#include "th02_clip_fixture.h"
#include "th02/draw/private.h"

#include "core/base/arena.h"

#include "test.h"

TEST(zoom_ignores_coverage_and_treats_color_zero_as_transparent)
{
    ARENA arena;
    Arena_Init(&arena, "zoom test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();

    Grcg_Init();
    uint8_t data[5] = { 0, 0x80, 0x40, 0, 0 };
    const PATTERN pattern = { .size = 0x0101,
                              .data = data,
                              .storage_size = sizeof(data) };
    memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
    Blit_Zoom(&pattern, g_Clip, 1, 1, 2);
    CHECK_EQ_INT(g_Video.pages[0][641], 1);
    CHECK_EQ_INT(g_Video.pages[0][2 * 640 + 2], 1);
    CHECK_EQ_INT(g_Video.pages[0][643], 2);
    CHECK_EQ_INT(g_Video.pages[0][2 * 640 + 4], 2);
    CHECK_EQ_INT(g_Video.pages[0][645], 9);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    CHECK_EQ_INT(g_Grcg->tiles[0], 0);
    CHECK_EQ_INT(g_Grcg->tiles[1], 255);
    data[0] = 255;
    Blit_Zoom(&pattern, g_Clip, 1, 1, 2);
    CHECK_EQ_INT(g_Video.pages[0][645], 9);
    Arena_Free(&arena);
}

TEST(zero_and_negative_scale_preserve_reversed_boxes_and_call_order)
{
    ARENA arena;
    Arena_Init(&arena, "zoom test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();

    Grcg_Init();
    uint8_t data[5] = { 0, 0x80, 0x40, 0, 0 };
    const PATTERN pattern = { .size = 0x0101,
                              .data = data,
                              .storage_size = sizeof(data) };
    memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
    Blit_Zoom(&pattern, g_Clip, 2, 2, 0);
    CHECK_EQ_INT(g_Video.pages[0][641], 2);
    CHECK_EQ_INT(g_Video.pages[0][2 * 640 + 2], 2);
    CHECK_EQ_INT(g_Video.pages[0][640], 9);
    memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
    Blit_Zoom(&pattern, g_Clip, 5, 5, -1);
    CHECK_EQ_INT(g_Video.pages[0][3 * 640 + 2], 2);
    CHECK_EQ_INT(g_Video.pages[0][5 * 640 + 4], 2);
    CHECK_EQ_INT(g_Video.pages[0][5 * 640 + 5], 1);
    CHECK_EQ_INT(g_Video.pages[0][5 * 640 + 1], 9);
    Arena_Free(&arena);
}

TEST(endpoint_overflow_and_empty_images_preserve_charger_and_page_rules)
{
    ARENA arena;
    Arena_Init(&arena, "zoom test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();

    Grcg_Init();
    uint8_t data[5] = { 0, 0x80, 0, 0, 0 };
    const PATTERN pattern = { .size = 0x0101,
                              .data = data,
                              .storage_size = sizeof(data) };
    Video_AccessPage(1);
    Blit_Zoom(&pattern, g_Clip, INT16_MAX, 0, 2);
    CHECK_EQ_INT(g_Video.pages[1][0], 1);
    CHECK_EQ_INT(g_Video.pages[1][2 * 640 - 1], 1);
    CHECK_EQ_INT(g_Video.pages[0][0], 0);
    memset(data, 0, sizeof(data));
    Grcg_SetColor(7);
    Blit_Zoom(&pattern, g_Clip, 0, 0, 3);
    CHECK_EQ_INT(g_Grcg->mode, 0);
    CHECK_EQ_INT(g_Grcg->tiles[0], 255);
    CHECK_EQ_INT(g_Grcg->tiles[1], 255);
    CHECK_EQ_INT(g_Grcg->tiles[2], 255);
    CHECK_EQ_INT(g_Grcg->tiles[3], 0);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    Arena_Free(&arena);
}
