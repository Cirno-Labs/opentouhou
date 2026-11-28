#include "tests/test.h"
#include "tests/th01/th01_video.h"
#include "th01/video.h"

#include "core/base/arena.h"

#include <string.h>

static void M_Init(ARENA *const arena)
{
    Arena_Init(arena, "video test", 2 * 1024 * 1024);
    Video_Init(Arena_Allocator(arena));
}

static int32_t M_Count(const uint8_t color)
{
    int32_t count = 0;
    for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        if (g_Video->pages[g_Video->drawing_page][i] == color) {
            count++;
        }
    }
    return count;
}

TEST(pages_belong_to_the_allocator)
{
    ARENA arena;
    M_Init(&arena);
    CHECK_EQ_INT(arena.used, 2 * VIDEO_PAGE_SIZE);
    CHECK_EQ_INT(g_Video->drawing_page, 0);
    CHECK_EQ_INT(g_Video->visible_page, 0);
    CHECK_EQ_INT(Th01Test_Pixel(1, 639, 399), 0);
    Video_Clear(0x23);
    Video_SelectVisiblePage(1);
    CHECK_EQ_INT(Th01Test_Pixel(0, 639, 399), 3);
    CHECK_EQ_INT(Th01Test_Pixel(1, 639, 399), 0);
    Video_SelectDrawingPage(1);
    Video_Clear(15);
    CHECK_EQ_INT(Th01Test_Pixel(0, 0, 0), 3);
    CHECK_EQ_INT(Th01Test_Pixel(1, 0, 0), 15);
    Arena_Reset(&arena);
    Video_Init(Arena_Allocator(&arena));
    CHECK_EQ_INT(arena.used, 2 * VIDEO_PAGE_SIZE);
    CHECK_EQ_INT(Th01Test_Pixel(0, 0, 0), 0);
    CHECK_EQ_INT(g_Video->visible_page, 0);
    Arena_Free(&arena);
}

TEST(primitives_clip_and_include_reversed_endpoints)
{
    ARENA arena;
    M_Init(&arena);
    Video_Pixel(-1, 0, 1);
    Video_Pixel(640, 0, 1);
    Video_Pixel(0, -1, 1);
    Video_Pixel(0, 400, 1);
    CHECK_EQ_INT(M_Count(1), 0);
    Video_HorizontalLine(3, -3, 0, 1);
    Video_VerticalLine(639, 403, 397, 2);
    CHECK_EQ_INT(M_Count(1), 4);
    CHECK_EQ_INT(M_Count(2), 3);
    Video_FillBox(2, 2, -2, -2, 0x23);
    CHECK_EQ_INT(M_Count(3), 9);
    Video_HorizontalLine(INT32_MIN, INT32_MAX, 400, 7);
    Video_VerticalLine(-1, INT32_MIN, INT32_MAX, 7);
    Video_FillBox(700, 0, 800, 399, 7);
    CHECK_EQ_INT(M_Count(7), 0);
    Arena_Free(&arena);
}

TEST(oblique_lines_preserve_rounding_and_unflushed_final_groups)
{
    ARENA arena;
    M_Init(&arena);
    // Enumerate the flushed points from the source's row/byte cursor.
    const int32_t ends[4][4] = {
        { 100, 100, 105, 102 },
        { 100, 102, 105, 100 },
        { 100, 100, 102, 105 },
        { 100, 105, 102, 100 },
    };
    const int32_t points[4][5][2] = {
        { { 100, 100 }, { 101, 100 }, { 102, 101 }, { 103, 101 } },
        { { 100, 102 }, { 101, 102 }, { 102, 101 }, { 103, 101 } },
        { { 100, 100 },
          { 100, 101 },
          { 101, 102 },
          { 101, 103 },
          { 102, 104 } },
        { { 100, 105 },
          { 100, 104 },
          { 101, 103 },
          { 101, 102 },
          { 102, 101 } },
    };
    for (int32_t octant = 0; octant < 8; octant++) {
        const int32_t index = octant % 4;
        const int32_t *const line = ends[index];
        const bool reverse = octant >= 4;
        Video_Clear(0);
        Video_Line(
            line[reverse ? 2 : 0], line[reverse ? 3 : 1], line[reverse ? 0 : 2],
            line[reverse ? 1 : 3], 7);
        const int32_t count = index < 2 ? 4 : 5;
        CHECK_EQ_INT(M_Count(7), count);
        for (int32_t i = 0; i < count; i++) {
            CHECK_EQ_INT(
                Th01Test_Pixel(0, points[index][i][0], points[index][i][1]), 7);
        }
    }
    Video_Clear(0);
    Video_Line(0, 0, 2, 1, 4);
    CHECK_EQ_INT(M_Count(4), 2);
    Video_Line(8, 10, 8, 12, 5);
    Video_Line(10, 10, 12, 10, 6);
    CHECK_EQ_INT(M_Count(5), 3);
    CHECK_EQ_INT(M_Count(6), 3);
    Arena_Free(&arena);
}

TEST(line_clipping_truncates_the_slope_before_scaling)
{
    ARENA arena;
    M_Init(&arena);
    Video_Line(-4, 10, 4, 12, 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 0, 10), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 1, 10), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 2, 11), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 3, 11), 3);
    CHECK_EQ_INT(M_Count(3), 4);
    Video_Clear(0);
    Video_Line(635, 10, 643, 12, 4);
    CHECK_EQ_INT(Th01Test_Pixel(0, 635, 10), 4);
    CHECK_EQ_INT(Th01Test_Pixel(0, 636, 10), 4);
    CHECK_EQ_INT(Th01Test_Pixel(0, 637, 11), 4);
    CHECK_EQ_INT(Th01Test_Pixel(0, 638, 11), 4);
    CHECK_EQ_INT(M_Count(4), 4);
    Video_Clear(0);
    Video_Line(10, -4, 12, 4, 5);
    CHECK_EQ_INT(Th01Test_Pixel(0, 10, 0), 5);
    CHECK_EQ_INT(Th01Test_Pixel(0, 10, 1), 5);
    CHECK_EQ_INT(Th01Test_Pixel(0, 11, 2), 5);
    CHECK_EQ_INT(Th01Test_Pixel(0, 11, 3), 5);
    CHECK_EQ_INT(M_Count(5), 4);
    Video_Clear(0);
    Video_Line(10, 395, 12, 403, 6);
    CHECK_EQ_INT(Th01Test_Pixel(0, 10, 395), 6);
    CHECK_EQ_INT(Th01Test_Pixel(0, 10, 396), 6);
    CHECK_EQ_INT(Th01Test_Pixel(0, 11, 397), 6);
    CHECK_EQ_INT(Th01Test_Pixel(0, 11, 398), 6);
    CHECK_EQ_INT(M_Count(6), 4);
    Video_Clear(0);
    Video_Line(-5, -5, -1, 405, 7);
    Video_Line(-5, -5, 645, -1, 7);
    Video_Line(640, 0, 650, 100, 7);
    Video_Line(0, 400, 100, 450, 7);
    CHECK_EQ_INT(M_Count(7), 0);
    Arena_Free(&arena);
}

TEST(lines_preserve_the_existing_six_directional_clip_cases)
{
    ARENA arena;
    M_Init(&arena);
    const int32_t cases[][6] = {
        { -8, 8, 16, 32, 0, 16 }, { 624, 0, 648, 24, 624, 0 },
        { 8, -8, 32, 16, 16, 0 }, { 0, 384, 24, 408, 0, 384 },
        { 0, 16, 24, -8, 0, 16 }, { 0, 408, 24, 384, 9, 399 },
    };
    for (int32_t i = 0; i < 6; i++) {
        Video_Clear(0);
        const int32_t *const line = cases[i];
        Video_Line(line[0], line[1], line[2], line[3], 3);
        CHECK_EQ_INT(Th01Test_Pixel(0, line[4], line[5]), 3);
    }
    Video_Clear(0);
    Video_Line(0, 0, 16, 16, 3);
    CHECK_EQ_INT(M_Count(3), 16);
    for (int32_t i = 0; i < 16; i++) {
        CHECK_EQ_INT(Th01Test_Pixel(0, i, i), 3);
    }
    CHECK_EQ_INT(Th01Test_Pixel(0, 16, 16), 0);
    Arena_Free(&arena);
}

TEST(indexed_blits_separate_coverage_key_and_opaque_zero)
{
    ARENA arena;
    M_Init(&arena);
    Video_Clear(9);
    const uint8_t pixels[] = { 0, 15, 3, 0xee, 4, 5, 6, 0xee };
    const uint8_t coverage[] = { 1, 0, 255, 1, 1, 1 };
    VIDEO_IMAGE image = {
        .pixels = pixels,
        .coverage = coverage,
        .width = 3,
        .height = 2,
        .stride = 4,
        .coverage_stride = 3,
        .key = -1,
    };
    Video_Blit(&image, (GFX_RECT) { 0, 0, 3, 2 }, 0, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 0, 0), 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 1, 0), 9);
    CHECK_EQ_INT(Th01Test_Pixel(0, 2, 0), 3);
    image.coverage = nullptr;
    Video_Blit(&image, (GFX_RECT) { 0, 0, 3, 2 }, 3, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 4, 0), 15);
    image.key = 15;
    Video_Blit(&image, (GFX_RECT) { 0, 0, 3, 2 }, 6, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 6, 0), 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 7, 0), 9);
    image.key = 0;
    Video_Blit(&image, (GFX_RECT) { 0, 0, 3, 2 }, 9, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 9, 0), 9);
    CHECK_EQ_INT(Th01Test_Pixel(0, 10, 0), 15);
    image.coverage = coverage;
    image.key = 3;
    Video_Blit(&image, (GFX_RECT) { 0, 0, 3, 2 }, 12, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 12, 0), 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 13, 0), 9);
    CHECK_EQ_INT(Th01Test_Pixel(0, 14, 0), 9);
    CHECK_EQ_INT(Th01Test_Pixel(0, 2, 1), 6);
    Arena_Free(&arena);
}

TEST(blits_and_masks_clip_source_and_destination_without_shifting)
{
    ARENA arena;
    M_Init(&arena);
    const uint8_t pixels[] = {
        0, 1, 2,  3,  0xee, 4,  5,  6,  7,  0xee,
        8, 9, 10, 11, 0xee, 12, 13, 14, 15, 0xee,
    };
    const VIDEO_IMAGE image = {
        .pixels = pixels,
        .width = 4,
        .height = 4,
        .stride = 5,
        .key = -1,
    };
    Video_Blit(&image, (GFX_RECT) { 2, 2, 2, 2 }, 10, 10);
    CHECK_EQ_INT(Th01Test_Pixel(0, 10, 10), 10);
    CHECK_EQ_INT(Th01Test_Pixel(0, 11, 11), 15);
    Video_Blit(&image, (GFX_RECT) { -1, -1, 4, 4 }, 20, 20);
    CHECK_EQ_INT(Th01Test_Pixel(0, 20, 20), 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 22, 22), 5);
    Video_Blit(&image, (GFX_RECT) { 0, 0, 4, 4 }, -2, -2);
    CHECK_EQ_INT(Th01Test_Pixel(0, 0, 0), 10);
    CHECK_EQ_INT(Th01Test_Pixel(0, 1, 1), 15);
    Video_Blit(&image, (GFX_RECT) { 2, 2, 4, 4 }, 638, 398);
    CHECK_EQ_INT(Th01Test_Pixel(0, 639, 399), 15);
    const VIDEO_MASK mask = { pixels, 4, 4, 5 };
    Video_Mask(&mask, (GFX_RECT) { 0, 0, 4, 4 }, -1, -1, 0x23);
    CHECK_EQ_INT(Th01Test_Pixel(0, 0, 0), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 2, 2), 3);
    Video_Mask(&mask, (GFX_RECT) { 0, 0, 2, 1 }, 30, 30, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 31, 30), 0);
    Video_Clear(5);
    Video_Mask(&mask, (GFX_RECT) { 0, 0, 2, 1 }, 30, 30, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 30, 30), 5);
    CHECK_EQ_INT(Th01Test_Pixel(0, 31, 30), 0);
    Video_Blit(&image, (GFX_RECT) { INT32_MIN, 0, INT32_MAX, 1 }, 0, 0);
    Video_Blit(&image, (GFX_RECT) { 0, 0, 4, 4 }, INT32_MAX, INT32_MIN);
    CHECK_EQ_INT(M_Count(5), VIDEO_PAGE_SIZE - 1);
    Arena_Free(&arena);
}

TEST(rectangle_copies_preserve_clipping_and_page_selection)
{
    ARENA arena;
    M_Init(&arena);
    Video_SelectDrawingPage(1);
    Video_Clear(7);
    Video_SelectVisiblePage(1);
    Video_Copy(1, (GFX_RECT) { -1, -1, 4, 4 }, 0, 10, 10);
    CHECK_EQ_INT(Th01Test_Pixel(0, 10, 10), 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 11, 11), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 13, 13), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 14, 13), 0);
    CHECK_EQ_INT(g_Video->drawing_page, 1);
    CHECK_EQ_INT(g_Video->visible_page, 1);
    Video_Copy(1, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 0, 0, 0);
    for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video->pages[0][i], 7);
    }
    Arena_Free(&arena);
}

TEST(overlapping_copies_read_the_original_rectangle)
{
    ARENA arena;
    M_Init(&arena);
    const uint8_t pixels[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
    const VIDEO_IMAGE image = {
        .pixels = pixels,
        .width = 4,
        .height = 3,
        .stride = 4,
        .key = -1,
    };
    const int32_t destinations[4][2] = {
        { 11, 11 }, { 9, 9 }, { 11, 10 }, { 9, 10 }
    };
    for (int32_t direction = 0; direction < 4; direction++) {
        Video_Clear(0);
        Video_Blit(&image, (GFX_RECT) { 0, 0, 4, 3 }, 10, 10);
        const int32_t x = destinations[direction][0];
        const int32_t y = destinations[direction][1];
        Video_Copy(0, (GFX_RECT) { 10, 10, 4, 3 }, 0, x, y);
        for (int32_t row = 0; row < 3; row++) {
            for (int32_t col = 0; col < 4; col++) {
                CHECK_EQ_INT(
                    Th01Test_Pixel(0, x + col, y + row), pixels[row * 4 + col]);
            }
        }
    }
    Arena_Free(&arena);
}

TEST(word_aligned_restore_rounds_the_original_right_edge)
{
    ARENA arena;
    M_Init(&arena);
    Video_SelectDrawingPage(1);
    Video_Clear(6);
    Video_Restore16((GFX_RECT) { 15, 10, 2, 1 });
    CHECK_EQ_INT(g_Video->drawing_page, 0);
    CHECK_EQ_INT(M_Count(6), 32);
    CHECK_EQ_INT(Th01Test_Pixel(0, 0, 10), 6);
    CHECK_EQ_INT(Th01Test_Pixel(0, 31, 10), 6);
    CHECK_EQ_INT(Th01Test_Pixel(0, 32, 10), 0);
    Video_Restore16((GFX_RECT) { 639, 399, 2, 10 });
    CHECK_EQ_INT(Th01Test_Pixel(0, 624, 399), 6);
    CHECK_EQ_INT(Th01Test_Pixel(0, 623, 399), 0);
    CHECK_EQ_INT(M_Count(6), 48);
    Video_SelectDrawingPage(1);
    Video_Restore16((GFX_RECT) { 0, 0, 0, 10 });
    CHECK_EQ_INT(g_Video->drawing_page, 0);
    Video_Restore16((GFX_RECT) { INT32_MAX, INT32_MIN, INT32_MAX, INT32_MAX });
    CHECK_EQ_INT(M_Count(6), 48);
    Arena_Free(&arena);
}

TEST(readback_preserves_padding_and_out_of_screen_destinations)
{
    ARENA arena;
    M_Init(&arena);
    Video_FillBox(0, 0, 1, 1, 7);
    uint8_t pixels[18];
    memset(pixels, 0xee, sizeof(pixels));
    Video_Read(0, (GFX_RECT) { -1, -1, 3, 3 }, pixels, 6);
    const uint8_t expected[] = {
        0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 7,    7,
        0xee, 0xee, 0xee, 0xee, 7,    7,    0xee, 0xee, 0xee,
    };
    CHECK(memcmp(pixels, expected, sizeof(expected)) == 0);
    Video_Read(0, (GFX_RECT) { 639, 399, 3, 3 }, pixels, 6);
    CHECK_EQ_INT(pixels[0], 0);
    CHECK_EQ_INT(pixels[1], 0xee);
    CHECK_EQ_INT(pixels[6], 0xee);
    CHECK_EQ_INT(g_Video->drawing_page, 0);
    CHECK_EQ_INT(g_Video->visible_page, 0);
    Arena_Free(&arena);
}

TEST(dac_masks_nibbles_and_keeps_color_zero_opaque)
{
    ARENA arena;
    M_Init(&arena);
    for (int32_t i = 0; i < 16; i++) {
        CHECK_EQ_INT(g_Video->palette[i].alpha, 255);
        CHECK_EQ_INT(g_Video->palette[i].red, 0);
    }
    Video_SetDac(0, -1, 16, 33);
    CHECK_EQ_INT(g_Video->palette[0].red, 255);
    CHECK_EQ_INT(g_Video->palette[0].green, 0);
    CHECK_EQ_INT(g_Video->palette[0].blue, 17);
    GFX_COLOR palette[16] = {};
    palette[0] = (GFX_COLOR) { 255, 159, 97, 0 };
    Video_SetPalette(palette);
    CHECK_EQ_INT(g_Video->palette[0].red, 255);
    CHECK_EQ_INT(g_Video->palette[0].green, 153);
    CHECK_EQ_INT(g_Video->palette[0].blue, 102);
    CHECK_EQ_INT(g_Video->palette[0].alpha, 255);
    Video_SetPalette(g_Video->palette);
    CHECK_EQ_INT(g_Video->palette[0].green, 153);
    CHECK_EQ_INT(g_Video->palette[0].alpha, 255);
    Arena_Free(&arena);
}
