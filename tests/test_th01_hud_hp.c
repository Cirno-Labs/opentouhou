#include "th01/hud_hp.h"
#include "core/base/arena.h"
#include "test.h"

TEST(pattern_mask_and_odd_boundary_decrement)
{
    ARENA arena;
    Arena_Init(&arena, "hp", 1024 * 1024);
    VIDEO video;
    Video_Init(&video, Arena_Allocator(&arena));
    HUD_HP hp;
    HudHp_Init(&hp, Arena_Allocator(&arena));
    hp.first_redwhite = 3;
    hp.first_white = 5;
    memset(video.pages[0], 15, VIDEO_PAGE_SIZE);
    memset(video.pages[1], 2, VIDEO_PAGE_SIZE);
    HudHp_Rerender(&hp, &video, 8);
    for (int32_t page = 0; page < 2; page++) {
        CHECK_EQ_INT(Video_ReadPixel(&video, page, 128, 48), 10);
        CHECK_EQ_INT(Video_ReadPixel(&video, page, 128 + 3 * 8, 48), 10);
        CHECK_EQ_INT(Video_ReadPixel(&video, page, 128 + 3 * 8 + 1, 48), 7);
        CHECK_EQ_INT(Video_ReadPixel(&video, page, 128 + 5 * 8, 48), 7);
        CHECK_EQ_INT(
            Video_ReadPixel(&video, page, 135, 48), page == 0 ? 15 : 2);
        CHECK_EQ_INT(
            Video_ReadPixel(&video, page, 128, 63), page == 0 ? 15 : 2);
    }
    CHECK_EQ_INT(hp.backgrounds[256], 15);
    HudHp_Decrement(&hp, &video, 3);
    for (int32_t page = 0; page < 2; page++) {
        CHECK_EQ_INT(Video_ReadPixel(&video, page, 144, 48), 10);
        CHECK_EQ_INT(Video_ReadPixel(&video, page, 145, 48), 7);
        CHECK_EQ_INT(Video_ReadPixel(&video, page, 152, 48), 15);
    }
    HudHp_Decrement(&hp, &video, 2);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 144, 48), 15);
    CHECK_EQ_INT(video.drawing_page, 0);
    Arena_Free(&arena);
}

TEST(snapshots_follow_selected_page_and_completion_is_equality)
{
    ARENA arena;
    Arena_Init(&arena, "hp", 1024 * 1024);
    VIDEO video;
    Video_Init(&video, Arena_Allocator(&arena));
    HUD_HP hp;
    HudHp_Init(&hp, Arena_Allocator(&arena));
    hp.first_redwhite = 4;
    hp.first_white = 8;
    memset(video.pages[0], 3, VIDEO_PAGE_SIZE);
    memset(video.pages[1], 6, VIDEO_PAGE_SIZE);
    Video_SelectDrawingPage(&video, 1);
    CHECK(!HudHp_Increment(&hp, &video, 8, 0));
    CHECK_EQ_INT(hp.backgrounds[0], 6);
    CHECK(!HudHp_Increment(&hp, &video, 8, 8));
    CHECK(HudHp_Increment(&hp, &video, 8, 7));
    CHECK_EQ_INT(hp.backgrounds[7 * 256], 3);
    HudHp_Decrement(&hp, &video, 0);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 128, 48), 6);
    CHECK_EQ_INT(Video_ReadPixel(&video, 1, 128, 48), 6);
    uint8_t before[16];
    memcpy(before, video.pages[0] + 48 * 640 + 128, 16);
    HudHp_Rerender(&hp, &video, 33);
    HudHp_Decrement(&hp, &video, -1);
    CHECK(!HudHp_Increment(&hp, &video, 8, 32));
    CHECK(memcmp(before, video.pages[0] + 48 * 640 + 128, 16) == 0);
    Arena_Free(&arena);
}
