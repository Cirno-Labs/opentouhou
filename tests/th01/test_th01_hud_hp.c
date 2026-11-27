#include "tests/test.h"
#include "tests/th01/th01_video.h"
#include "tests/th01/th01_world.h"
#include "th01/hud_hp.c"
#include "th01/hud_hp.h"

#include "core/base/arena.h"

#include <string.h>

TEST(pattern_mask_and_odd_boundary_decrement)
{
    Th01Test_ResetWorld();
    ARENA arena;
    Arena_Init(&arena, "hp", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    HudHp_Init(Arena_Allocator(&arena));
    HudHp_Begin(3, 5);
    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 2, VIDEO_PAGE_SIZE);
    HudHp_Rerender(8);
    for (int32_t page = 0; page < 2; page++) {
        CHECK_EQ_INT(Th01Test_Pixel(page, 128, 48), 10);
        CHECK_EQ_INT(Th01Test_Pixel(page, 128 + 3 * 8, 48), 10);
        CHECK_EQ_INT(Th01Test_Pixel(page, 128 + 3 * 8 + 1, 48), 7);
        CHECK_EQ_INT(Th01Test_Pixel(page, 128 + 5 * 8, 48), 7);
        CHECK_EQ_INT(Th01Test_Pixel(page, 135, 48), page == 0 ? 15 : 2);
        CHECK_EQ_INT(Th01Test_Pixel(page, 128, 63), page == 0 ? 15 : 2);
    }
    CHECK_EQ_INT(m_Health.backgrounds[256], 15);
    HudHp_Decrement(3);
    for (int32_t page = 0; page < 2; page++) {
        CHECK_EQ_INT(Th01Test_Pixel(page, 144, 48), 10);
        CHECK_EQ_INT(Th01Test_Pixel(page, 145, 48), 7);
        CHECK_EQ_INT(Th01Test_Pixel(page, 152, 48), 15);
    }
    HudHp_Decrement(2);
    CHECK_EQ_INT(Th01Test_Pixel(0, 144, 48), 15);
    CHECK_EQ_INT(g_Video.drawing_page, 0);
    Arena_Free(&arena);
}

TEST(snapshots_follow_selected_page_and_completion_is_equality)
{
    Th01Test_ResetWorld();
    ARENA arena;
    Arena_Init(&arena, "hp", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    HudHp_Init(Arena_Allocator(&arena));
    HudHp_Begin(4, 8);
    memset(g_Video.pages[0], 3, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 6, VIDEO_PAGE_SIZE);
    Video_SelectDrawingPage(1);
    CHECK(!HudHp_Increment(8, 0));
    CHECK_EQ_INT(m_Health.backgrounds[0], 6);
    CHECK(!HudHp_Increment(8, 8));
    CHECK(HudHp_Increment(8, 7));
    CHECK_EQ_INT(m_Health.backgrounds[7 * 256], 3);
    HudHp_Decrement(0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 128, 48), 6);
    CHECK_EQ_INT(Th01Test_Pixel(1, 128, 48), 6);
    uint8_t before[16];
    memcpy(before, g_Video.pages[0] + 48 * 640 + 128, 16);
    HudHp_Rerender(33);
    HudHp_Decrement(-1);
    CHECK(!HudHp_Increment(8, 32));
    CHECK(memcmp(before, g_Video.pages[0] + 48 * 640 + 128, 16) == 0);
    Arena_Free(&arena);
}
