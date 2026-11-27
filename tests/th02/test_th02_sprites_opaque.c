#include "tests/test.h"
#include "th02/draw/private.h"

#include "core/base/arena.h"

#include <string.h>

TEST(opaque_copies_ignore_coverage_round_x_and_overwrite_zero)
{
    ARENA arena;
    Arena_Init(&arena, "opaque sprite test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    uint8_t data[10] = { 0, 0, 0x80, 0, 0x40, 0, 0x20, 0, 0x10, 0 };
    const PATTERN pattern = { .size = 0x0102,
                              .data = data,
                              .storage_size = sizeof(data) };
    Video_AccessPage(1);
    memset(g_Video.pages[1], 15, VIDEO_PAGE_SIZE);
    Blit_OverPut8(&pattern, 7, 0);
    CHECK_EQ_INT(g_Video.pages[1][0], 1);
    CHECK_EQ_INT(g_Video.pages[1][1], 2);
    CHECK_EQ_INT(g_Video.pages[1][2], 4);
    CHECK_EQ_INT(g_Video.pages[1][3], 8);
    CHECK_EQ_INT(g_Video.pages[1][4], 0);
    CHECK_EQ_INT(g_Video.pages[1][8], 15);
    CHECK_EQ_INT(g_Video.pages[1][640], 0);
    CHECK_EQ_INT(g_Video.pages[0][0], 0);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    Arena_Free(&arena);
}

TEST(hidden_rows_and_physical_plane_aliases_are_preserved)
{
    ARENA arena;
    Arena_Init(&arena, "opaque sprite test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    uint8_t data[10] = { 0, 0, 0x80, 0x80, 0x40, 0x40, 0x20, 0x20, 0x10, 0x10 };
    const PATTERN pattern = { .size = 0x0102,
                              .data = data,
                              .storage_size = sizeof(data) };
    Blit_OverPut8(&pattern, 0, 400);
    CHECK_EQ_INT(g_Video.pages[0][VIDEO_VISIBLE_SIZE], 1);
    CHECK_EQ_INT(g_Video.pages[0][VIDEO_VISIBLE_SIZE + 1], 2);
    memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
    Blit_OverPut8(&pattern, 0, -1);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 0, VIDEO_PLANE_SIZE - 80), 255);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 1, VIDEO_PLANE_SIZE - 80), 0x80);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 2, VIDEO_PLANE_SIZE - 80), 0x40);
    CHECK_EQ_INT(Video_ReadPlaneByte(0, 3, VIDEO_PLANE_SIZE - 80), 255);
    CHECK_EQ_INT(g_Video.pages[0][0], 1);
    CHECK_EQ_INT(g_Video.pages[0][1], 2);
    Arena_Free(&arena);
}
