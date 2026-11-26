#include "th02/draw/private.h"

#include "core/base/arena.h"

#include "test.h"

TEST(source_planes_and_selected_destination_bits_are_independent)
{
    ARENA arena;
    Arena_Init(&arena, "plane test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    uint8_t data[10] = { 0x80, 0, 0x40, 0, 0x20, 0, 0x10, 0, 8, 0 };
    const PATTERN pattern = { .size = 0x0102,
                              .data = data,
                              .storage_size = sizeof(data) };
    for (uint8_t plane = 0; plane < 5; plane++) {
        for (uint16_t disabled = 0; disabled < 16; disabled++) {
            memset(g_Video.pages[0], 5, VIDEO_PAGE_SIZE);
            Blit_PutPlane(&pattern, 0, 0, plane, 0xffc0 | disabled);
            for (size_t x = 0; x < 8; x++) {
                CHECK_EQ_INT(
                    g_Video.pages[0][x],
                    x == plane ? (5 | (15 ^ disabled)) : 5);
            }
            memset(g_Video.pages[0], 15, VIDEO_PAGE_SIZE);
            Blit_PutPlane(&pattern, 0, 0, plane, 0x00c0 | disabled);
            CHECK_EQ_INT(g_Video.pages[0][plane], disabled);
        }
    }
    Arena_Free(&arena);
}

TEST(tile_alignment_and_rolling_preserve_pages_and_display_state)
{
    ARENA arena;
    Arena_Init(&arena, "plane test", 1024 * 1024);
    Video_Init(Arena_Allocator(&arena));
    uint8_t data[10] = { 255, 255 };
    const PATTERN pattern = { .size = 0x0102,
                              .data = data,
                              .storage_size = sizeof(data) };
    Video_AccessPage(1);
    Video_SetSplit(77, 123, 456);
    memset(g_Video.pages[1], 2, VIDEO_PAGE_SIZE);
    Blit_RollPutPlane(&pattern, 639, 399, 0, 0xa5ce);
    CHECK_EQ_INT(g_Video.pages[1][VIDEO_VISIBLE_SIZE - 1], 3);
    CHECK_EQ_INT(g_Video.pages[1][VIDEO_VISIBLE_SIZE], 3);
    CHECK_EQ_INT(g_Video.pages[1][VIDEO_VISIBLE_SIZE + 1], 2);
    CHECK_EQ_INT(g_Video.pages[1][639], 3);
    CHECK_EQ_INT(g_Video.pages[1][640], 3);
    CHECK_EQ_INT(g_Video.pages[0][639], 0);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.first_lines, 77);
    CHECK_EQ_INT(g_Video.second_address, 456);
    Arena_Free(&arena);
}
