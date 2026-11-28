#include "tests/test.h"
#include "tests/th01/th01_video.h"
#include "th01/wave.h"

#include "core/base/arena.h"

#include <string.h>

TEST(quarter_cycle_rows_and_zero_height)
{
    ARENA arena;
    Arena_Init(&arena, "wave", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video->pages[1], 3, VIDEO_PAGE_SIZE);
    Video_SelectDrawingPage(1);
    Wave_Restore(32, 100, 4, 16, 0, 8, 0);
    CHECK_EQ_INT(g_Video->drawing_page, 1);
    CHECK_EQ_INT(Th01Test_Pixel(0, 32, 100), 0);
    Wave_Restore(32, 100, 4, 16, 0, 8, 4);
    CHECK_EQ_INT(g_Video->drawing_page, 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 32, 100), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 48, 101), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 32, 102), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 16, 103), 3);
    CHECK_EQ_INT(Th01Test_Pixel(0, 31, 100), 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 32, 101), 0);
    Arena_Free(&arena);
}

TEST(negative_phase_step_and_truncated_displacement)
{
    ARENA arena;
    Arena_Init(&arena, "wave", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video->pages[1], 7, VIDEO_PAGE_SIZE);
    Wave_Restore(16, 100, -4, -1, 1, 1, 2);
    CHECK_EQ_INT(Th01Test_Pixel(0, 16, 100), 7);
    CHECK_EQ_INT(Th01Test_Pixel(0, 0, 100), 0);
    CHECK_EQ_INT(Th01Test_Pixel(0, 16, 101), 7);
    Arena_Free(&arena);
}

TEST(wrapped_coordinates_restore_at_planar_boundary)
{
    ARENA arena;
    Arena_Init(&arena, "wave", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    memset(g_Video->pages[1], 10, VIDEO_PAGE_SIZE);
    Wave_Restore(16, 410, 257, 0, 32767, 8, 1);
    CHECK_EQ_INT(Th01Test_Pixel(0, 272, 0), 10);
    CHECK_EQ_INT(Th01Test_Pixel(0, 287, 0), 10);
    CHECK_EQ_INT(Th01Test_Pixel(0, 288, 0), 0);
    Arena_Free(&arena);
}
