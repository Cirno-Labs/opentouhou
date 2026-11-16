#include "th01/video.h"

#include "core/base/arena.h"

#include "test.h"

#include <string.h>

static void M_Init(ARENA *const arena, VIDEO *const video)
{
    Arena_Init(arena, "video test", 2 * 1024 * 1024);
    Video_Init(video, Arena_Allocator(arena));
}

static int32_t M_Count(const VIDEO *const video, const uint8_t color)
{
    int32_t count = 0;
    for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        if (video->pages[video->drawing_page][i] == color) {
            count++;
        }
    }
    return count;
}

TEST(pages_and_contexts_share_only_the_caller_lifetime)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    VIDEO other;
    Video_Init(&other, Arena_Allocator(&arena));
    CHECK_EQ_INT(arena.used, 4 * VIDEO_PAGE_SIZE);
    CHECK_EQ_INT(video.drawing_page, 0);
    CHECK_EQ_INT(video.visible_page, 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 1, 639, 399), 0);
    Video_Clear(&video, 0x23);
    Video_SelectVisiblePage(&video, 1);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 639, 399), 3);
    CHECK_EQ_INT(Video_ReadPixel(&video, 1, 639, 399), 0);
    Video_SelectDrawingPage(&video, 1);
    Video_Clear(&video, 15);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 0, 0), 3);
    CHECK_EQ_INT(Video_ReadPixel(&video, 1, 0, 0), 15);
    CHECK_EQ_INT(Video_ReadPixel(&other, 1, 0, 0), 0);
    CHECK_EQ_INT(other.visible_page, 0);
    Arena_Reset(&arena);
    Video_Init(&video, Arena_Allocator(&arena));
    CHECK_EQ_INT(arena.used, 2 * VIDEO_PAGE_SIZE);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 0, 0), 0);
    Arena_Free(&arena);
}

TEST(primitives_clip_and_include_reversed_endpoints)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    Video_Pixel(&video, -1, 0, 1);
    Video_Pixel(&video, 640, 0, 1);
    Video_Pixel(&video, 0, -1, 1);
    Video_Pixel(&video, 0, 400, 1);
    CHECK_EQ_INT(M_Count(&video, 1), 0);
    Video_HorizontalLine(&video, 3, -3, 0, 1);
    Video_VerticalLine(&video, 639, 403, 397, 2);
    CHECK_EQ_INT(M_Count(&video, 1), 4);
    CHECK_EQ_INT(M_Count(&video, 2), 3);
    Video_FillBox(&video, 2, 2, -2, -2, 0x23);
    CHECK_EQ_INT(M_Count(&video, 3), 9);
    Video_Box(&video, 5, 5, 3, 3, 4);
    CHECK_EQ_INT(M_Count(&video, 4), 8);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 4, 4), 0);
    Video_HorizontalLine(&video, INT32_MIN, INT32_MAX, 400, 7);
    Video_VerticalLine(&video, -1, INT32_MIN, INT32_MAX, 7);
    Video_FillBox(&video, 700, 0, 800, 399, 7);
    CHECK_EQ_INT(M_Count(&video, 7), 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, INT32_MAX, INT32_MIN), 0);
    Arena_Free(&arena);
}

TEST(oblique_lines_preserve_rounding_and_unflushed_final_groups)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
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
        Video_Clear(&video, 0);
        Video_Line(
            &video, line[reverse ? 2 : 0], line[reverse ? 3 : 1],
            line[reverse ? 0 : 2], line[reverse ? 1 : 3], 7);
        const int32_t count = index < 2 ? 4 : 5;
        CHECK_EQ_INT(M_Count(&video, 7), count);
        for (int32_t i = 0; i < count; i++) {
            CHECK_EQ_INT(
                Video_ReadPixel(
                    &video, 0, points[index][i][0], points[index][i][1]),
                7);
        }
    }
    Video_Clear(&video, 0);
    Video_Line(&video, 0, 0, 2, 1, 4);
    CHECK_EQ_INT(M_Count(&video, 4), 2);
    Video_Line(&video, 8, 10, 8, 12, 5);
    Video_Line(&video, 10, 10, 12, 10, 6);
    CHECK_EQ_INT(M_Count(&video, 5), 3);
    CHECK_EQ_INT(M_Count(&video, 6), 3);
    Arena_Free(&arena);
}

TEST(line_clipping_truncates_the_slope_before_scaling)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    Video_Line(&video, -4, 10, 4, 12, 3);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 0, 10), 3);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 1, 10), 3);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 2, 11), 3);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 3, 11), 3);
    CHECK_EQ_INT(M_Count(&video, 3), 4);
    Video_Clear(&video, 0);
    Video_Line(&video, 635, 10, 643, 12, 4);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 635, 10), 4);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 636, 10), 4);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 637, 11), 4);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 638, 11), 4);
    CHECK_EQ_INT(M_Count(&video, 4), 4);
    Video_Clear(&video, 0);
    Video_Line(&video, 10, -4, 12, 4, 5);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 10, 0), 5);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 10, 1), 5);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 11, 2), 5);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 11, 3), 5);
    CHECK_EQ_INT(M_Count(&video, 5), 4);
    Video_Clear(&video, 0);
    Video_Line(&video, 10, 395, 12, 403, 6);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 10, 395), 6);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 10, 396), 6);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 11, 397), 6);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 11, 398), 6);
    CHECK_EQ_INT(M_Count(&video, 6), 4);
    Video_Clear(&video, 0);
    Video_Line(&video, -5, -5, -1, 405, 7);
    Video_Line(&video, -5, -5, 645, -1, 7);
    Video_Line(&video, 640, 0, 650, 100, 7);
    Video_Line(&video, 0, 400, 100, 450, 7);
    CHECK_EQ_INT(M_Count(&video, 7), 0);
    Arena_Free(&arena);
}

TEST(lines_preserve_the_existing_six_directional_clip_cases)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    const int32_t cases[][6] = {
        { -8, 8, 16, 32, 0, 16 }, { 624, 0, 648, 24, 624, 0 },
        { 8, -8, 32, 16, 16, 0 }, { 0, 384, 24, 408, 0, 384 },
        { 0, 16, 24, -8, 0, 16 }, { 0, 408, 24, 384, 9, 399 },
    };
    for (int32_t i = 0; i < 6; i++) {
        Video_Clear(&video, 0);
        const int32_t *const line = cases[i];
        Video_Line(&video, line[0], line[1], line[2], line[3], 3);
        CHECK_EQ_INT(Video_ReadPixel(&video, 0, line[4], line[5]), 3);
    }
    Video_Clear(&video, 0);
    Video_Line(&video, 0, 0, 16, 16, 3);
    CHECK_EQ_INT(M_Count(&video, 3), 16);
    for (int32_t i = 0; i < 16; i++) {
        CHECK_EQ_INT(Video_ReadPixel(&video, 0, i, i), 3);
    }
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 16, 16), 0);
    Arena_Free(&arena);
}

TEST(indexed_blits_separate_coverage_key_and_opaque_zero)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    Video_Clear(&video, 9);
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
    Video_Blit(&video, &image, (GFX_RECT) { 0, 0, 3, 2 }, 0, 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 0, 0), 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 1, 0), 9);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 2, 0), 3);
    image.coverage = nullptr;
    Video_Blit(&video, &image, (GFX_RECT) { 0, 0, 3, 2 }, 3, 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 4, 0), 15);
    image.key = 15;
    Video_Blit(&video, &image, (GFX_RECT) { 0, 0, 3, 2 }, 6, 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 6, 0), 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 7, 0), 9);
    image.key = 0;
    Video_Blit(&video, &image, (GFX_RECT) { 0, 0, 3, 2 }, 9, 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 9, 0), 9);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 10, 0), 15);
    image.coverage = coverage;
    image.key = 3;
    Video_Blit(&video, &image, (GFX_RECT) { 0, 0, 3, 2 }, 12, 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 12, 0), 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 13, 0), 9);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 14, 0), 9);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 2, 1), 6);
    Arena_Free(&arena);
}

TEST(blits_and_masks_clip_source_and_destination_without_shifting)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
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
    Video_Blit(&video, &image, (GFX_RECT) { 2, 2, 2, 2 }, 10, 10);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 10, 10), 10);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 11, 11), 15);
    Video_Blit(&video, &image, (GFX_RECT) { -1, -1, 4, 4 }, 20, 20);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 20, 20), 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 22, 22), 5);
    Video_Blit(&video, &image, (GFX_RECT) { 0, 0, 4, 4 }, -2, -2);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 0, 0), 10);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 1, 1), 15);
    Video_Blit(&video, &image, (GFX_RECT) { 2, 2, 4, 4 }, 638, 398);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 639, 399), 15);
    const VIDEO_MASK mask = { pixels, 4, 4, 5 };
    Video_Mask(&video, &mask, (GFX_RECT) { 0, 0, 4, 4 }, -1, -1, 0x23);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 0, 0), 3);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 2, 2), 3);
    Video_Mask(&video, &mask, (GFX_RECT) { 0, 0, 2, 1 }, 30, 30, 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 31, 30), 0);
    Video_Clear(&video, 5);
    Video_Mask(&video, &mask, (GFX_RECT) { 0, 0, 2, 1 }, 30, 30, 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 30, 30), 5);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 31, 30), 0);
    Video_Blit(&video, &image, (GFX_RECT) { INT32_MIN, 0, INT32_MAX, 1 }, 0, 0);
    Video_Blit(&video, &image, (GFX_RECT) { 0, 0, 4, 4 }, INT32_MAX, INT32_MIN);
    CHECK_EQ_INT(M_Count(&video, 5), VIDEO_PAGE_SIZE - 1);
    Arena_Free(&arena);
}

TEST(rectangle_copies_preserve_clipping_and_page_selection)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    Video_SelectDrawingPage(&video, 1);
    Video_Clear(&video, 7);
    Video_SelectVisiblePage(&video, 1);
    Video_Copy(&video, 1, (GFX_RECT) { -1, -1, 4, 4 }, 0, 10, 10);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 10, 10), 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 11, 11), 7);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 13, 13), 7);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 14, 13), 0);
    CHECK_EQ_INT(video.drawing_page, 1);
    CHECK_EQ_INT(video.visible_page, 1);
    Video_Copy(
        &video, 1, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 0, 0, 0);
    for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(video.pages[0][i], 7);
    }
    Arena_Free(&arena);
}

TEST(overlapping_copies_read_the_original_rectangle)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
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
        Video_Clear(&video, 0);
        Video_Blit(&video, &image, (GFX_RECT) { 0, 0, 4, 3 }, 10, 10);
        const int32_t x = destinations[direction][0];
        const int32_t y = destinations[direction][1];
        Video_Copy(&video, 0, (GFX_RECT) { 10, 10, 4, 3 }, 0, x, y);
        for (int32_t row = 0; row < 3; row++) {
            for (int32_t col = 0; col < 4; col++) {
                CHECK_EQ_INT(
                    Video_ReadPixel(&video, 0, x + col, y + row),
                    pixels[row * 4 + col]);
            }
        }
    }
    Arena_Free(&arena);
}

TEST(word_aligned_restore_rounds_the_original_right_edge)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    Video_SelectDrawingPage(&video, 1);
    Video_Clear(&video, 6);
    Video_Restore16(&video, (GFX_RECT) { 15, 10, 2, 1 });
    CHECK_EQ_INT(video.drawing_page, 0);
    CHECK_EQ_INT(M_Count(&video, 6), 32);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 0, 10), 6);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 31, 10), 6);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 32, 10), 0);
    Video_Restore16(&video, (GFX_RECT) { 639, 399, 2, 10 });
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 624, 399), 6);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 623, 399), 0);
    CHECK_EQ_INT(M_Count(&video, 6), 48);
    Video_SelectDrawingPage(&video, 1);
    Video_Restore16(&video, (GFX_RECT) { 0, 0, 0, 10 });
    CHECK_EQ_INT(video.drawing_page, 0);
    Video_Restore16(
        &video, (GFX_RECT) { INT32_MAX, INT32_MIN, INT32_MAX, INT32_MAX });
    CHECK_EQ_INT(M_Count(&video, 6), 48);
    Arena_Free(&arena);
}

TEST(readback_preserves_padding_and_out_of_screen_destinations)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    Video_FillBox(&video, 0, 0, 1, 1, 7);
    uint8_t pixels[18];
    memset(pixels, 0xee, sizeof(pixels));
    Video_Read(&video, 0, (GFX_RECT) { -1, -1, 3, 3 }, pixels, 6);
    const uint8_t expected[] = {
        0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 7,    7,
        0xee, 0xee, 0xee, 0xee, 7,    7,    0xee, 0xee, 0xee,
    };
    CHECK(memcmp(pixels, expected, sizeof(expected)) == 0);
    Video_Read(&video, 0, (GFX_RECT) { 639, 399, 3, 3 }, pixels, 6);
    CHECK_EQ_INT(pixels[0], 0);
    CHECK_EQ_INT(pixels[1], 0xee);
    CHECK_EQ_INT(pixels[6], 0xee);
    CHECK_EQ_INT(video.drawing_page, 0);
    CHECK_EQ_INT(video.visible_page, 0);
    Arena_Free(&arena);
}

TEST(dac_masks_nibbles_and_keeps_color_zero_opaque)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    for (int32_t i = 0; i < 16; i++) {
        CHECK_EQ_INT(video.palette[i].alpha, 255);
        CHECK_EQ_INT(video.palette[i].red, 0);
    }
    Video_SetDac(&video, 0, -1, 16, 33);
    CHECK_EQ_INT(video.palette[0].red, 255);
    CHECK_EQ_INT(video.palette[0].green, 0);
    CHECK_EQ_INT(video.palette[0].blue, 17);
    GFX_COLOR palette[16] = {};
    palette[0] = (GFX_COLOR) { 255, 159, 97, 0 };
    Video_SetPalette(&video, palette);
    CHECK_EQ_INT(video.palette[0].red, 255);
    CHECK_EQ_INT(video.palette[0].green, 153);
    CHECK_EQ_INT(video.palette[0].blue, 102);
    CHECK_EQ_INT(video.palette[0].alpha, 255);
    Video_SetPalette(&video, video.palette);
    CHECK_EQ_INT(video.palette[0].green, 153);
    CHECK_EQ_INT(video.palette[0].alpha, 255);
    Arena_Free(&arena);
}
