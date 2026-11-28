#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "th02/draw.h"
#include "th02/grcg_private.h"

#include "core/base/arena.h"
#include "core/base/memory.h"

#include <string.h>

TEST(rmw_orders_endpoints_and_clips_inclusive_edges)
{
    ARENA arena;
    Arena_Init(&arena, "GRCG test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();
    TestClip_Set(5, 7, 13, 8);

    Grcg_Init();
    Grcg_SetColor(7);
    Draw_BoxFill(g_Clip, 20, 20, 0, 0);
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        const size_t x = pixel % VIDEO_WIDTH;
        const size_t y = pixel / VIDEO_WIDTH;
        CHECK_EQ_INT(
            g_Video.pages[0][pixel],
            x >= 5 && x <= 13 && y >= 7 && y <= 8 ? 7 : 0);
    }
    Arena_Free(&arena);
}

TEST(color_and_off_writes_keep_distinct_edge_masks)
{
    ARENA arena;
    Arena_Init(&arena, "GRCG test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();

    Grcg_Init();
    memset(g_Video.pages[0], 2, VIDEO_PAGE_SIZE);
    Grcg_SetColor(1);
    Grcg_SetPlanes(1);
    Draw_BoxFill(g_Clip, 5, 0, 5, 0);
    for (size_t x = 0; x < 17; x++) {
        CHECK_EQ_INT(g_Video.pages[0][x], x == 5 ? 3 : 2);
    }
    Grcg_Off();
    CHECK_EQ_INT(p_Grcg->tiles[0], 255);
    Draw_BoxFill(g_Clip, 5, 0, 5, 0);
    for (size_t x = 0; x < 16; x++) {
        CHECK_EQ_INT(g_Video.pages[0][x], x == 5 ? 3 : 2);
    }
    Arena_Free(&arena);
}

TEST(relative_coordinate_overflow_and_tile_patterns_are_preserved)
{
    ARENA arena;
    Arena_Init(&arena, "GRCG test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();
    TestClip_Set(32, 0, 447, 399);

    Grcg_Init();
    Grcg_SetColor(15);
    Draw_BoxFill(g_Clip, INT16_MIN, 0, 639, 0);
    CHECK_EQ_INT(g_Video.pages[0][32], 0);
    p_Grcg->mode = 0xce;
    p_Grcg->tiles[0] = 0xa5;
    Video_AccessPage(1);
    Draw_BoxFill(g_Clip, 32, 0, 39, 0);
    CHECK_EQ_INT(Video_ReadPlaneByte(1, 0, 4), 0xa5);
    CHECK_EQ_INT(Video_ReadPlaneByte(1, 1, 4), 0);
    CHECK_EQ_INT(g_Video.pages[0][32], 0);
    CHECK_EQ_INT(p_Grcg->mode, 0xce);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    Arena_Free(&arena);
}

TEST(horizontal_lines_order_relative_coordinates_after_signed_wrap)
{
    ARENA arena;
    Arena_Init(&arena, "GRCG line test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();
    TestClip_Set(32, 7, 447, 8);

    Grcg_SetColor(5);
    Draw_HLine(g_Clip, INT16_MIN, 0, 7);
    Draw_HLine(g_Clip, 0, 639, 6);
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        const size_t x = pixel % 640;
        const size_t y = pixel / 640;
        CHECK_EQ_INT(
            g_Video.pages[0][pixel], y == 7 && x >= 32 && x <= 447 ? 5 : 0);
    }
    Arena_Free(&arena);
}

TEST(rounded_corners_follow_original_integer_circle_rows)
{
    ARENA arena;
    Arena_Init(&arena, "GRCG round test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();

    Grcg_SetColor(7);
    Draw_RoundBoxFill(g_Clip, 16, 16, 32, 32, 8);
    const size_t inset[17] = {
        6, 4, 3, 2, 1, 1, 0, 0, 0, 0, 0, 1, 1, 2, 3, 4, 6
    };
    for (size_t y = 0; y < 48; y++) {
        for (size_t x = 0; x < 48; x++) {
            const bool covered = y >= 16 && y <= 32 && x >= 16 + inset[y - 16]
                && x <= 32 - inset[y - 16];
            CHECK_EQ_INT(g_Video.pages[0][y * 640 + x], covered ? 7 : 0);
        }
    }
    CHECK_EQ_INT(p_Grcg->mode, 0xc0);
    Arena_Free(&arena);
}

TEST(rounded_panels_clamp_radius_order_endpoints_and_preserve_other_page)
{
    ARENA arena;
    Arena_Init(&arena, "GRCG radius test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();
    TestClip_Set(5, 7, 13, 8);

    Grcg_SetColor(1);
    Grcg_SetPlanes(1);
    p_Grcg->tiles[0] = 0xa5;
    const GRCG before = *p_Grcg;
    memset(g_Video.pages[0], 6, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 2, VIDEO_PAGE_SIZE);
    Video_AccessPage(1);
    Draw_RoundBoxFill(g_Clip, 13, 9, 5, 7, UINT16_MAX);
    CHECK_EQ_INT(Video_ReadPlaneByte(1, 0, 7 * 80), 0x05);
    CHECK_EQ_INT(Video_ReadPlaneByte(1, 0, 8 * 80), 0x05);
    Draw_RoundBoxFill(g_Clip, 0, 0, 20, 20, 0);
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        const size_t x = pixel % 640;
        const size_t y = pixel / 640;
        const bool covered = x >= 5 && x <= 13 && y >= 7 && y <= 8;
        const uint8_t expected =
            covered && (0xa5 & (0x80u >> (x % 8))) != 0 ? 3 : 2;
        CHECK_EQ_INT(g_Video.pages[1][pixel], expected);
        CHECK_EQ_INT(g_Video.pages[0][pixel], 6);
    }
    CHECK(memcmp(p_Grcg, &before, sizeof(*p_Grcg)) == 0);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    Arena_Free(&arena);
}

TEST(lines_use_half_unit_rounding_in_both_directions)
{
    ARENA arena;
    Arena_Init(&arena, "GRCG lines", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();

    Grcg_SetColor(5);
    const int16_t offsets[][2] = {
        { 0, 0 }, { 1, 1 }, { 2, 1 }, { 3, 2 }, { 4, 2 }
    };
    for (int axis = 0; axis < 2; axis++) {
        for (int direction = -1; direction <= 1; direction += 2) {
            for (int reversed = 0; reversed < 2; reversed++) {
                memset(g_Video.pages[1], 0, VIDEO_PAGE_SIZE);
                Video_AccessPage(1);
                const int16_t x = 20 + (axis == 0 ? 4 : 2);
                const int16_t y = 20 + direction * (axis == 0 ? 2 : 4);
                Draw_Line(
                    g_Clip, reversed ? x : 20, reversed ? y : 20,
                    reversed ? 20 : x, reversed ? 20 : y);
                for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
                    bool covered = false;
                    for (size_t point = 0; point < ARRAY_SIZE(offsets);
                         point++) {
                        const size_t px = 20 + offsets[point][axis];
                        const size_t py =
                            20 + direction * offsets[point][1 - axis];
                        covered |= i == py * VIDEO_WIDTH + px;
                    }
                    CHECK_EQ_INT(g_Video.pages[1][i], covered ? 5 : 0);
                    CHECK_EQ_INT(g_Video.pages[0][i], 0);
                }
                CHECK_EQ_INT(g_Video.drawing_page, 1);
                CHECK_EQ_INT(p_Grcg->mode, 0xc0);
            }
        }
    }
    Arena_Free(&arena);
}

TEST(vertical_lines_clip_and_write_bytes_instead_of_words)
{
    ARENA arena;
    Arena_Init(&arena, "GRCG vertical lines", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    TestClip_Set(3, 7, 10, 9);
    const uint8_t modes[] = { 0, 0xce };
    for (size_t m = 0; m < ARRAY_SIZE(modes); m++) {
        memset(g_Video.pages[0], 3, VIDEO_PAGE_SIZE);
        Grcg_Init();
        p_Grcg->mode = modes[m];
        memcpy(
            p_Grcg->tiles, (const uint8_t[4]) { 0xa5, 0, 0, 0 },
            sizeof(p_Grcg->tiles));
        const GRCG before = *p_Grcg;
        Draw_VLine(g_Clip, 5, 20, -2);
        Draw_VLine(g_Clip, 2, 7, 9);
        for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
            const size_t x = i % VIDEO_WIDTH;
            const size_t y = i / VIDEO_WIDTH;
            uint8_t expected = 3;
            if (y >= 7 && y <= 9 && x < 8) {
                uint8_t mask = 255;
                if (modes[m] == 0) {
                    mask = 4;
                }
                expected = 2 | ((mask >> (7 - x)) & 1);
            }
            CHECK_EQ_INT(g_Video.pages[0][i], expected);
        }
        CHECK(memcmp(p_Grcg, &before, sizeof(*p_Grcg)) == 0);
    }
    Arena_Free(&arena);
}

TEST(line_points_write_bytes_and_shallow_lines_write_words)
{
    ARENA arena;
    Arena_Init(&arena, "GRCG line write widths", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();
    const uint8_t modes[] = { 0, 0xce };
    for (size_t m = 0; m < ARRAY_SIZE(modes); m++) {
        memset(g_Video.pages[0], 3, VIDEO_PAGE_SIZE);
        Grcg_Init();
        p_Grcg->mode = modes[m];
        memcpy(
            p_Grcg->tiles, (const uint8_t[4]) { 0xa5, 0, 0, 0 },
            sizeof(p_Grcg->tiles));
        Draw_Line(g_Clip, 5, 1, 5, 1);
        Draw_Line(g_Clip, 5, 2, 6, 2);
        for (size_t x = 0; x < 24; x++) {
            const uint8_t point_blue = modes[m] == 0
                ? x == 5
                : x != 5 || ((0xa5 >> (7 - (x & 7))) & 1);
            const uint8_t line_blue = modes[m] == 0
                ? x == 5 || x == 6
                : (x != 5 && x != 6) || ((0xa5 >> (7 - (x & 7))) & 1);
            CHECK_EQ_INT(g_Video.pages[0][640 + x], x < 8 ? 2 | point_blue : 3);
            CHECK_EQ_INT(
                g_Video.pages[0][1280 + x], x < 16 ? 2 | line_blue : 3);
        }
    }
    Arena_Free(&arena);
}

TEST(tdw_lines_and_filled_circles_write_whole_addressed_bytes)
{
    ARENA arena;
    Arena_Init(&arena, "GRCG TDW writes", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    Clip_Reset();
    Grcg_SetColor(1);
    p_Grcg->mode = 0x8e;
    p_Grcg->tiles[0] = 0xa5;

    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    Draw_HLine(g_Clip, 4, 4, 7);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 0, 7 * 80), 0xa5);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 0, 7 * 80 + 1), 0xa5);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 0, 7 * 80 + 2), 255);
    for (uint8_t plane = 1; plane < 4; plane++) {
        CHECK_EQ_INT(Video_ReadPlaneByte(0, plane, 7 * 80), 255);
        CHECK_EQ_INT(Video_ReadPlaneByte(0, plane, 7 * 80 + 1), 255);
    }

    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    Draw_CircleFill(4, 9, 0);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 0, 9 * 80), 0xa5);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 0, 9 * 80 + 1), 255);

    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    Draw_CircleFill(4, 11, 1);
    for (int y = 10; y <= 12; y++) {
        CHECK_EQ_INT(Video_ReadPlaneByte(0, 0, y * 80), 0xa5);
        CHECK_EQ_INT(Video_ReadPlaneByte(0, 0, y * 80 + 1), 0xa5);
        CHECK_EQ_INT(Video_ReadPlaneByte(0, 0, y * 80 + 2), 255);
    }
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 0, 9 * 80), 255);
    CHECK_EQ_INT(p_Grcg->mode, 0x8e);
    CHECK_EQ_INT(p_Grcg->tiles[0], 0xa5);
    Arena_Free(&arena);
}

TEST(rectangles_keep_inclusive_clip_endpoints_and_untouched_storage)
{
    const MATH16_VECTOR vertices[2][4] = {
        { { 4, 4 }, { 36, 4 }, { 36, 24 }, { 4, 24 } },
        { { 4, 24 }, { 36, 24 }, { 36, 4 }, { 4, 4 } },
    };
    for (size_t winding = 0; winding < 2; winding++) {
        Video_Init(&g_HeapAllocator);
        memset(g_Video.pages[0], 6, VIDEO_PAGE_SIZE * 2);
        g_Video.drawing_page = 1;
        g_Video.visible_page = 0;
        TestClip_Set(9, 8, 30, 20);

        Grcg_SetColor(1);
        Grcg_SetPlanes(1);
        const GRCG before = *g_Grcg;
        Draw_PolygonFill(g_Clip, vertices[winding], 4);
        for (size_t index = 0; index < VIDEO_PAGE_SIZE; index++) {
            const size_t x = index % 640;
            const size_t y = index / 640;
            const bool inside = x >= 9 && x <= 30 && y >= 8 && y <= 20;
            CHECK_EQ_INT(g_Video.pages[0][index], 6);
            CHECK_EQ_INT(g_Video.pages[1][index], inside ? 7 : 6);
        }
        CHECK_EQ_INT(g_Video.drawing_page, 1);
        CHECK_EQ_INT(g_Video.visible_page, 0);
        CHECK(memcmp(g_Grcg, &before, sizeof(*g_Grcg)) == 0);
        Mem_Free(g_Video.pages[0]);
    }
}

TEST(clipping_at_a_vertex_keeps_the_zero_height_segment_side_effect)
{
    Video_Init(&g_HeapAllocator);
    TestClip_Set(0, 8, 63, 31);
    const MATH16_VECTOR vertices[] = { { 16, 0 }, { 32, 8 }, { 0, 16 } };

    Grcg_SetColor(1);
    Grcg_SetPlanes(1);
    Draw_PolygonFill(g_Clip, vertices, 3);
    CHECK_EQ_INT(g_Video.pages[0][8 * 640 + 6], 0);
    CHECK_EQ_INT(g_Video.pages[0][8 * 640 + 7], 1);
    CHECK_EQ_INT(g_Video.pages[0][8 * 640 + 32], 1);
    CHECK_EQ_INT(g_Video.pages[0][8 * 640 + 33], 0);
    CHECK_EQ_INT(g_Video.pages[0][9 * 640 + 5], 0);
    CHECK_EQ_INT(g_Video.pages[0][9 * 640 + 6], 1);
    CHECK_EQ_INT(g_Video.pages[0][16 * 640], 1);
    CHECK_EQ_INT(g_Video.pages[0][17 * 640], 0);
    Mem_Free(g_Video.pages[0]);
}

TEST(raw_writes_replace_words_while_color_writes_mask_edges)
{
    Video_Init(&g_HeapAllocator);

    Clip_Reset();
    const MATH16_VECTOR line[] = { { 2, 10 }, { 4, 10 } };

    Grcg_Init();
    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    Draw_PolygonFill(g_Clip, line, 2);
    for (size_t x = 0; x < 32; x++) {
        CHECK_EQ_INT(
            g_Video.pages[0][10 * 640 + x],
            x >= 16 || (x >= 2 && x <= 4) ? 15 : 14);
    }
    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    Grcg_SetColor(5);
    Grcg_SetPlanes(15);
    Draw_PolygonFill(g_Clip, line, 2);
    for (size_t x = 0; x < 32; x++) {
        CHECK_EQ_INT(g_Video.pages[0][10 * 640 + x], x >= 2 && x <= 4 ? 5 : 15);
    }
    Mem_Free(g_Video.pages[0]);
}

TEST(coarse_rejection_preserves_pages_and_the_vertex_list)
{
    Video_Init(&g_HeapAllocator);
    memset(g_Video.pages[0], 11, VIDEO_PAGE_SIZE * 2);

    Clip_Reset();

    Grcg_SetColor(0);
    const MATH16_VECTOR vertices[] = {
        { -120, 10 }, { -20, 40 }, { -20, 100 }, { -120, 130 }
    };
    MATH16_VECTOR before[4];
    memcpy(before, vertices, sizeof(before));
    Draw_PolygonFill(g_Clip, vertices, 4);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE * 2; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], 11);
    }
    CHECK(memcmp(vertices, before, sizeof(before)) == 0);
    Mem_Free(g_Video.pages[0]);
}
