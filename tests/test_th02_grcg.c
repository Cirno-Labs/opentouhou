#include "th02_clip_fixture.h"
#include "../src/th02/grcg.c"
#include "th02/grcg.h"

#include "core/base/arena.h"

#include "test.h"

TEST(rmw_orders_endpoints_and_clips_inclusive_edges)
{
    ARENA arena;
    Arena_Init(&arena, "GRCG test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));

    Clip_Reset();
    TestClip_Set(5, 7, 13, 8);

    Grcg_Init();
    Grcg_SetColor(7);
    Grcg_BoxFill(g_Clip, 20, 20, 0, 0);
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        const size_t x = pixel % VIDEO_WIDTH, y = pixel / VIDEO_WIDTH;
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
    Grcg_BoxFill(g_Clip, 5, 0, 5, 0);
    for (size_t x = 0; x < 17; x++) {
        CHECK_EQ_INT(g_Video.pages[0][x], x == 5 ? 3 : 2);
    }
    Grcg_Off();
    CHECK_EQ_INT(m_Charger.tiles[0], 255);
    Grcg_BoxFill(g_Clip, 5, 0, 5, 0);
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
    Grcg_BoxFill(g_Clip, INT16_MIN, 0, 639, 0);
    CHECK_EQ_INT(g_Video.pages[0][32], 0);
    m_Charger.mode = 0xce;
    m_Charger.tiles[0] = 0xa5;
    Video_AccessPage(1);
    Grcg_BoxFill(g_Clip, 32, 0, 39, 0);
    CHECK_EQ_INT(Video_ReadPlaneByte(1, 0, 4), 0xa5);
    CHECK_EQ_INT(Video_ReadPlaneByte(1, 1, 4), 0);
    CHECK_EQ_INT(g_Video.pages[0][32], 0);
    CHECK_EQ_INT(m_Charger.mode, 0xce);
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
    Grcg_HLine(g_Clip, INT16_MIN, 0, 7);
    Grcg_HLine(g_Clip, 0, 639, 6);
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        const size_t x = pixel % 640, y = pixel / 640;
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
    Grcg_RoundBoxFill(g_Clip, 16, 16, 32, 32, 8);
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
    CHECK_EQ_INT(m_Charger.mode, 0xc0);
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
    m_Charger.tiles[0] = 0xa5;
    const GRCG before = m_Charger;
    memset(g_Video.pages[0], 6, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 2, VIDEO_PAGE_SIZE);
    Video_AccessPage(1);
    Grcg_RoundBoxFill(g_Clip, 13, 9, 5, 7, UINT16_MAX);
    CHECK_EQ_INT(Video_ReadPlaneByte(1, 0, 7 * 80), 0x05);
    CHECK_EQ_INT(Video_ReadPlaneByte(1, 0, 8 * 80), 0x05);
    Grcg_RoundBoxFill(g_Clip, 0, 0, 20, 20, 0);
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        const size_t x = pixel % 640, y = pixel / 640;
        const bool covered = x >= 5 && x <= 13 && y >= 7 && y <= 8;
        const uint8_t expected =
            covered && (0xa5 & (0x80u >> (x % 8))) != 0 ? 3 : 2;
        CHECK_EQ_INT(g_Video.pages[1][pixel], expected);
        CHECK_EQ_INT(g_Video.pages[0][pixel], 6);
    }
    CHECK(memcmp(&m_Charger, &before, sizeof(m_Charger)) == 0);
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
                Grcg_Line(
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
                CHECK_EQ_INT(m_Charger.mode, 0xc0);
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
        m_Charger.mode = modes[m];
        memcpy(
            m_Charger.tiles, (const uint8_t[4]) { 0xa5, 0, 0, 0 },
            sizeof(m_Charger.tiles));
        const GRCG before = m_Charger;
        Grcg_VLine(g_Clip, 5, 20, -2);
        Grcg_VLine(g_Clip, 2, 7, 9);
        for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
            const size_t x = i % VIDEO_WIDTH, y = i / VIDEO_WIDTH;
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
        CHECK(memcmp(&m_Charger, &before, sizeof(m_Charger)) == 0);
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
        m_Charger.mode = modes[m];
        memcpy(
            m_Charger.tiles, (const uint8_t[4]) { 0xa5, 0, 0, 0 },
            sizeof(m_Charger.tiles));
        Grcg_Line(g_Clip, 5, 1, 5, 1);
        Grcg_Line(g_Clip, 5, 2, 6, 2);
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
