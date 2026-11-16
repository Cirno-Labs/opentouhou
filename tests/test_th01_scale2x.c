#include "th01/scale2x.h"

#include "core/base/arena.h"

#include "test.h"

TEST(transparent_cells_and_byte_rounding)
{
    ARENA arena;
    Arena_Init(&arena, "scale2x", 1024 * 1024);
    VIDEO video;
    Video_Init(&video, Arena_Allocator(&arena));
    memset(video.pages[0], 9, VIDEO_PAGE_SIZE);
    for (int32_t x = 0; x < 32; x++) {
        video.pages[1][3 * VIDEO_WIDTH + 8 + x] = (uint8_t)(x % 16);
        video.pages[1][4 * VIDEO_WIDTH + 8 + x] = (uint8_t)(15 - x % 16);
    }
    uint8_t *const source = Arena_Alloc(&arena, VIDEO_PAGE_SIZE);
    memcpy(source, video.pages[1], VIDEO_PAGE_SIZE);
    Video_SelectDrawingPage(&video, 1);
    Video_SelectVisiblePage(&video, 1);
    Video_SetDac(&video, 4, 1, 2, 3);
    Scale2x_Copy(&video, 15, 11, 9, 3, 31, 2);
    CHECK_EQ_INT(video.drawing_page, 0);
    CHECK_EQ_INT(video.visible_page, 1);
    CHECK_EQ_INT(video.palette[4].red, 17);
    CHECK(memcmp(source, video.pages[1], VIDEO_PAGE_SIZE) == 0);
    for (int32_t y = 0; y < VIDEO_HEIGHT; y++) {
        for (int32_t x = 0; x < VIDEO_WIDTH; x++) {
            uint8_t expected = 9;
            if (x >= 8 && x < 40 && y >= 11 && y < 15) {
                const uint8_t ink =
                    (uint8_t)((y - 11) / 2 == 0 ? (x - 8) / 2
                                                : 15 - (x - 8) / 2);
                if (ink != 0) {
                    expected = ink;
                }
            }
            CHECK_EQ_INT(video.pages[0][y * VIDEO_WIDTH + x], expected);
        }
    }
    Arena_Free(&arena);
}

TEST(empty_words_preserve_selector)
{
    ARENA arena;
    Arena_Init(&arena, "scale2x", 1024 * 1024);
    VIDEO video;
    Video_Init(&video, Arena_Allocator(&arena));
    Video_SelectDrawingPage(&video, 1);
    Scale2x_Copy(&video, 0, 0, 0, 0, 15, 16);
    Scale2x_Copy(&video, 0, 0, 0, 0, 16, 0);
    CHECK_EQ_INT(video.drawing_page, 1);
    Scale2x_Copy(&video, 0, 0, 0, 0, 16, 1);
    CHECK_EQ_INT(video.drawing_page, 0);
    Arena_Free(&arena);
}

TEST(last_source_row_and_destination_word)
{
    ARENA arena;
    Arena_Init(&arena, "scale2x", 1024 * 1024);
    VIDEO video;
    Video_Init(&video, Arena_Allocator(&arena));
    memset(video.pages[0], 6, VIDEO_PAGE_SIZE);
    video.pages[1][399 * VIDEO_WIDTH + 624] = 1;
    video.pages[1][399 * VIDEO_WIDTH + 639] = 4;
    Scale2x_Copy(&video, 615, 398, 631, 399, 16, 1);
    CHECK_EQ_INT(video.pages[0][398 * VIDEO_WIDTH + 608], 1);
    CHECK_EQ_INT(video.pages[0][399 * VIDEO_WIDTH + 609], 1);
    CHECK_EQ_INT(video.pages[0][398 * VIDEO_WIDTH + 610], 6);
    CHECK_EQ_INT(video.pages[0][399 * VIDEO_WIDTH + 638], 4);
    CHECK_EQ_INT(video.pages[0][399 * VIDEO_WIDTH + 639], 4);
    CHECK_EQ_INT(video.pages[0][397 * VIDEO_WIDTH + 639], 6);
    Arena_Free(&arena);
}
