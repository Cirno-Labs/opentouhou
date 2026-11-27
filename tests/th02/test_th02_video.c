#include "tests/test.h"
#include "tests/th02/th02_clip_fixture.h"
#include "th02/video.h"

#include "core/base/arena.h"

static void M_Init(ARENA *const arena)
{
    Arena_Init(arena, "TH02 video test", 1024 * 1024);
    Video_Init(Arena_Allocator(arena));
}

static uint8_t M_Index(const size_t pixel)
{
    return (uint8_t)((pixel / 640 + pixel / 16 + pixel % 13) & 15);
}

TEST(initializes_independent_pages_and_opaque_black_palette)
{
    ARENA arena;
    M_Init(&arena);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.first_lines, 400);
    CHECK_EQ_INT(g_Video.first_address, 0);
    CHECK_EQ_INT(g_Video.second_address, 0);
    CHECK(g_Video.pages[0] != g_Video.pages[1]);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], 0);
        CHECK_EQ_INT(g_Video.pages[1][i], 0);
    }
    for (size_t i = 0; i < 16; i++) {
        CHECK_EQ_INT(g_Video.palette[i].red, 0);
        CHECK_EQ_INT(g_Video.palette[i].green, 0);
        CHECK_EQ_INT(g_Video.palette[i].blue, 0);
        CHECK_EQ_INT(g_Video.palette[i].alpha, 255);
    }
    Video_AccessPage(255);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    Video_ShowPage(3);
    Video_AccessPage(2);
    CHECK_EQ_INT(g_Video.visible_page, 1);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    Arena_Free(&arena);
}

TEST(clear_and_page_copy_preserve_hidden_tails_and_display_state)
{
    ARENA arena;
    M_Init(&arena);
    memset(g_Video.pages[0], 3, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 12, VIDEO_PAGE_SIZE);
    Video_AccessPage(1);
    Video_ShowPage(1);
    Video_SetSplit(100, 123, 456);
    Video_SetDac(0, 1, 2, 3);
    Video_Clear();
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], 3);
        CHECK_EQ_INT(g_Video.pages[1][i], i < VIDEO_VISIBLE_SIZE ? 0 : 12);
    }
    Video_CopyPage(3);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[1][i], i < VIDEO_VISIBLE_SIZE ? 3 : 12);
    }
    memset(g_Video.pages[1], 7, VIDEO_VISIBLE_SIZE);
    Video_CopyPage(2);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], i < VIDEO_VISIBLE_SIZE ? 7 : 3);
    }
    CHECK_EQ_INT(g_Video.visible_page, 1);
    CHECK_EQ_INT(g_Video.first_lines, 100);
    CHECK_EQ_INT(g_Video.first_address, 123);
    CHECK_EQ_INT(g_Video.second_address, 456);
    CHECK_EQ_INT(g_Video.palette[0].red, 17);
    Arena_Free(&arena);
}

TEST(plane_byte_writes_preserve_other_planes_on_both_pages)
{
    ARENA arena;
    M_Init(&arena);
    const uint16_t offsets[] = { 0, 1, 79, 80, 31999, 32000, 32767 };
    for (uint8_t page = 0; page < 2; page++) {
        Video_AccessPage(page);
        for (uint8_t plane = 0; plane < 4; plane++) {
            for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
                const uint16_t offset = offsets[i];
                memset(g_Video.pages[page] + (size_t)offset * 8, 15, 8);
                for (unsigned value = 0; value < 256; value++) {
                    Video_WritePlaneByte(plane, offset, value);
                    CHECK_EQ_INT(
                        Video_ReadPlaneByte(page + 2, plane, offset), value);
                    for (uint8_t other = 0; other < 4; other++) {
                        if (other != plane) {
                            CHECK_EQ_INT(
                                Video_ReadPlaneByte(page, other, offset), 255);
                        }
                    }
                }
            }
        }
    }
    CHECK_EQ_INT(g_Video.visible_page, 0);
    Arena_Free(&arena);
}

TEST(circular_scroll_keeps_every_source_row_in_order)
{
    ARENA arena;
    M_Init(&arena);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        g_Video.pages[1][i] = M_Index(i);
    }
    Video_ShowPage(1);
    uint8_t *const output = Mem_Alloc(VIDEO_VISIBLE_SIZE);
    for (uint16_t top = 0; top < 400; top++) {
        Video_ScrollUp(top);
        CHECK_EQ_INT(g_Video.first_lines, 400 - top);
        CHECK_EQ_INT(g_Video.first_address, top * 40);
        CHECK_EQ_INT(g_Video.second_address, 0);
        Video_Scanout(output, VIDEO_WIDTH);
        for (size_t y = 0; y < 400; y++) {
            const size_t source = (y + top) % 400 * VIDEO_WIDTH;
            CHECK_EQ_INT(output[y * VIDEO_WIDTH], M_Index(source));
            CHECK_EQ_INT(output[y * VIDEO_WIDTH + 639], M_Index(source + 639));
        }
    }
    Mem_Free(output);
    Arena_Free(&arena);
}

TEST(split_horizontal_pan_reads_hidden_pixels_and_preserves_output_padding)
{
    ARENA arena;
    M_Init(&arena);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        g_Video.pages[0][i] = M_Index(i);
    }
    Video_SetSplit(300, 100 * 40 + 1, 1);
    const size_t stride = VIDEO_WIDTH + 5;
    uint8_t *const output = Mem_Alloc(stride * VIDEO_HEIGHT);
    memset(output, 0xcc, stride * VIDEO_HEIGHT);
    Video_Scanout(output, stride);
    for (size_t y = 0; y < VIDEO_HEIGHT; y++) {
        const size_t source = y < 300 ? (y + 100) * VIDEO_WIDTH + 16
                                      : (y - 300) * VIDEO_WIDTH + 16;
        for (size_t x = 0; x < VIDEO_WIDTH; x++) {
            CHECK_EQ_INT(output[y * stride + x], M_Index(source + x));
        }
        for (size_t x = VIDEO_WIDTH; x < stride; x++) {
            CHECK_EQ_INT(output[y * stride + x], 0xcc);
        }
    }
    CHECK_EQ_INT(output[299 * stride + 639], M_Index(VIDEO_VISIBLE_SIZE + 15));
    Mem_Free(output);
    Arena_Free(&arena);
}

TEST(zero_length_and_out_of_range_scroll_keep_the_first_region_address)
{
    ARENA arena;
    M_Init(&arena);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        g_Video.pages[0][i] = M_Index(i);
    }
    uint8_t *const output = Mem_Alloc(VIDEO_VISIBLE_SIZE);
    const uint16_t tops[] = { 400, 401, UINT16_MAX };
    for (size_t i = 0; i < sizeof(tops) / sizeof(tops[0]); i++) {
        Video_ScrollUp(tops[i]);
        CHECK_EQ_INT(g_Video.first_lines, 0);
        CHECK_EQ_INT(g_Video.first_address, 16000);
        Video_Scanout(output, VIDEO_WIDTH);
        for (size_t pixel = 0; pixel < VIDEO_VISIBLE_SIZE; pixel++) {
            const size_t source =
                (VIDEO_VISIBLE_SIZE + pixel) % VIDEO_PAGE_SIZE;
            CHECK_EQ_INT(output[pixel], M_Index(source));
        }
    }
    Video_SetSplit(UINT16_MAX, UINT16_MAX, 0);
    CHECK_EQ_INT(g_Video.first_lines, 400);
    CHECK_EQ_INT(g_Video.first_address, UINT16_MAX);
    Video_Scanout(output, VIDEO_WIDTH);
    for (size_t pixel = 0; pixel < VIDEO_VISIBLE_SIZE; pixel++) {
        CHECK_EQ_INT(
            output[pixel],
            M_Index((UINT16_MAX * 16u + pixel) % VIDEO_PAGE_SIZE));
    }
    Mem_Free(output);
    Arena_Free(&arena);
}

TEST(dac_masks_raw_components_and_keeps_index_zero_opaque)
{
    ARENA arena;
    M_Init(&arena);
    for (uint8_t i = 0; i < 16; i++) {
        Video_SetDac(i, 0x10 + i, 0x2f - i, 0x85);
        CHECK_EQ_INT(g_Video.palette[i].red, i * 17);
        CHECK_EQ_INT(g_Video.palette[i].green, (15 - i) * 17);
        CHECK_EQ_INT(g_Video.palette[i].blue, 85);
        CHECK_EQ_INT(g_Video.palette[i].alpha, 255);
    }
    CHECK_EQ_INT(g_Video.first_lines, 400);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    Arena_Free(&arena);
}

TEST(image_draw_rounds_width_and_x_and_wraps_at_screen_bottom)
{
    ARENA arena;
    M_Init(&arena);
    memset(g_Video.pages[0], 7, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 11, VIDEO_PAGE_SIZE);
    Video_AccessPage(1);
    Video_ShowPage(1);
    Video_SetSplit(123, 456, 789);
    Video_SetDac(3, 1, 2, 3);
    TestClip_Set(300, 0, 310, 399);
    const uint8_t pixels[30] = {
        0,  1,  2,  3, 4, 5,  6,  7,  8,  9,  8,  9, 10, 11, 12,
        13, 14, 15, 0, 1, 15, 14, 13, 12, 11, 10, 9, 8,  7,  6,
    };
    Video_PutImage8(g_Clip, pixels, 10, 3, 15, 399);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], 7);
        uint8_t expected = 11;
        const size_t y = i / 640;
        const size_t x = i % 640;
        if (x >= 8 && x < 16 && (y == 399 || y < 2)) {
            const size_t row = y == 399 ? 0 : y + 1;
            expected = pixels[row * 10 + x - 8];
        }
        CHECK_EQ_INT(g_Video.pages[1][i], expected);
    }
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 1);
    CHECK_EQ_INT(g_Video.first_lines, 123);
    CHECK_EQ_INT(g_Video.first_address, 456);
    CHECK_EQ_INT(g_Video.second_address, 789);
    CHECK_EQ_INT(g_Video.palette[3].red, 17);
    CHECK_EQ_INT(g_Video.palette[3].green, 34);
    CHECK_EQ_INT(g_Video.palette[3].blue, 51);
    Arena_Free(&arena);
}

TEST(image_draw_clips_y_and_screen_right_and_accepts_empty_images)
{
    ARENA arena;
    M_Init(&arena);
    uint8_t pixels[16 * 403];
    for (size_t row = 0; row < 403; row++) {
        memset(pixels + row * 16, row & 15, 16);
    }
    TestClip_Set(100, 1, 200, 2);
    Video_PutImage8(g_Clip, pixels, 16, 403, 639, 399);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        const size_t y = i / 640;
        const size_t x = i % 640;
        const uint8_t expected =
            x >= 632 && (y == 1 || y == 2) ? ((y == 1 ? 402 : 3) & 15) : 0;
        CHECK_EQ_INT(g_Video.pages[0][i], expected);
    }
    uint8_t before[VIDEO_PAGE_SIZE];
    memcpy(before, g_Video.pages[0], sizeof(before));
    Video_PutImage8(g_Clip, nullptr, 0, 10, 0, 0);
    Video_PutImage8(g_Clip, nullptr, 16, 0, 0, 0);
    Video_PutImage8(g_Clip, nullptr, 6, 10, 0, 0);
    Video_PutImage8(g_Clip, nullptr, 16, 10, 640, 0);
    CHECK(memcmp(before, g_Video.pages[0], sizeof(before)) == 0);
    Arena_Free(&arena);
}

TEST(rect_restore_rounds_coordinates_and_reaches_hidden_rows_without_rolling)
{
    ARENA arena;
    M_Init(&arena);
    memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    g_Video.drawing_page = g_Video.visible_page = 1;
    Video_SetSplit(123, 456, 789);
    Video_RestoreRect16(15, 398, 31, 4);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        const size_t y = i / 640;
        const size_t x = i % 640;
        CHECK_EQ_INT(
            g_Video.pages[0][i],
            y >= 398 && y < 402 && x >= 8 && x < 24 ? 6 : 9);
        CHECK_EQ_INT(g_Video.pages[1][i], 6);
    }
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    CHECK_EQ_INT(g_Video.visible_page, 1);
    CHECK_EQ_INT(g_Video.first_lines, 123);
    CHECK_EQ_INT(g_Video.first_address, 456);
    CHECK_EQ_INT(g_Video.second_address, 789);
    Arena_Free(&arena);
}

TEST(rect_restore_preserves_empty_page_selection_and_raw_negative_x_carry)
{
    ARENA arena;
    M_Init(&arena);
    memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    g_Video.drawing_page = 1;
    Video_RestoreRect16(0, 0, 15, 10);
    Video_RestoreRect16(0, 0, -16, 10);
    Video_RestoreRect16(0, 0, 16, 0);
    Video_RestoreRect16(0, 410, 16, 1);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], 9);
    }
    Video_RestoreRect16(-1, 1, 16, 1);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], i >= 632 && i < 648 ? 6 : 9);
    }
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    Arena_Free(&arena);
}

TEST(rect_restore_skips_negative_row_offsets_and_keeps_plane_aliases)
{
    ARENA arena;
    M_Init(&arena);
    memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    Video_RestoreRect16(376, 409, 16, 2);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(
            g_Video.pages[0][i],
            i < 8                          ? 15
                : i >= VIDEO_PAGE_SIZE - 8 ? 6
                                           : 9);
    }
    memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
    Video_RestoreRect16(0, -1, 16, 2);
    for (size_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], i < 16 ? 6 : 9);
    }
    Arena_Free(&arena);
}
