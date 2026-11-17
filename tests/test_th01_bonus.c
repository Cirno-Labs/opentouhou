#include "th01/bonus.h"

#include "core/base/arena.h"

#include "test.h"

TEST(rowshift_preserves_pixel_order_and_source)
{
    ARENA arena;
    Arena_Init(&arena, "bonus", 1024 * 1024);
    VIDEO video;
    Video_Init(&video, Arena_Allocator(&arena));
    for (int32_t y = 0; y < 2; y++) {
        for (int32_t x = 0; x < VIDEO_WIDTH; x++) {
            video.pages[0][y * VIDEO_WIDTH + x] = (uint8_t)(x % 15);
            video.pages[1][y * VIDEO_WIDTH + x] = (uint8_t)(x / 32 % 16);
        }
    }
    for (uint16_t step = 0; step < 20; step++) {
        for (uint16_t y = 0; y < 2; y++) {
            Bonus_RowShift32(&video, y, step * 4);
            const int32_t inserted = (step + 1) * 32;
            for (int32_t x = 0; x < VIDEO_WIDTH; x++) {
                const int32_t old_x = y == 0 ? x + inserted : x - inserted;
                const int32_t new_x = y == 0 ? x - VIDEO_WIDTH + inserted
                                             : x + VIDEO_WIDTH - inserted;
                const uint8_t expected = old_x >= 0 && old_x < VIDEO_WIDTH
                    ? (uint8_t)(old_x % 15)
                    : (uint8_t)(new_x / 32 % 16);
                CHECK_EQ_INT(Video_ReadPixel(&video, 0, x, y), expected);
                CHECK_EQ_INT(Video_ReadPixel(&video, 1, x, y), x / 32 % 16);
            }
        }
    }
    CHECK_EQ_INT(video.drawing_page, 0);
    Arena_Free(&arena);
}

TEST(row_transition_empty_scan_fades_and_final_delay)
{
    ARENA arena;
    Arena_Init(&arena, "bonus", 2 * 1024 * 1024);
    const ALLOCATOR *const a = Arena_Allocator(&arena);
    VIDEO video;
    Video_Init(&video, a);
    memset(video.pages[0], 3, VIDEO_PAGE_SIZE);
    memset(video.pages[1], 7, VIDEO_PAGE_SIZE);
    PALETTE_STATE palette = {};
    PALETTE target;
    memset(&target, 15, sizeof(target));
    BONUS_ROW_TRANSITION t = {};
    FRAME_VSYNC vsync = {};
    Bonus_BeginRowTransition(&t);
    CHECK(!Bonus_ResumeRowTransition(&t, &video, &palette, &target, &vsync));
    CHECK_EQ_INT(t.activated, 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 639, 0), 3);
    for (int32_t step = 0; step < 70; step++) {
        const BONUS_ROW_TRANSITION frozen = t;
        CHECK(
            !Bonus_ResumeRowTransition(&t, &video, &palette, &target, &vsync));
        CHECK(memcmp(&frozen, &t, sizeof(t)) == 0);
        vsync.count1 = 2;
        const bool ready =
            Bonus_ResumeRowTransition(&t, &video, &palette, &target, &vsync);
        CHECK(ready == (step == 69));
        CHECK_EQ_INT(vsync.count1, 0);
    }
    CHECK_EQ_INT(t.done, 400);
    CHECK(memcmp(video.pages[0], video.pages[1], VIDEO_PAGE_SIZE) == 0);
    CHECK_EQ_INT(palette.saved.colors[0][0], 15);
    CHECK_EQ_INT(video.palette[0].red, 255);
    CHECK(Bonus_ResumeRowTransition(&t, &video, &palette, &target, &vsync));
    Arena_Free(&arena);
}
