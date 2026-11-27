#include "tests/test.h"
#include "th02/draw/private.h"

#include "core/base/arena.h"

static void M_Init(ARENA *const arena)
{
    Arena_Init(arena, "tiny sprite test", 1024 * 1024);
    Video_Init(Arena_Allocator(arena));
}

TEST(ordered_records_replace_covered_pixels_and_allow_color_zero)
{
    ARENA arena;
    M_Init(&arena);
    uint8_t data[160] = {};
    data[0] = 0x80;
    data[1] = 3;
    data[2] = 0x80;
    data[34] = 0x80;
    data[36] = 0xc0;
    const PATTERN pattern = { .size = 0x0210,
                              .data = data,
                              .storage_size = sizeof(data) };
    Video_AccessPage(1);
    Video_SetSplit(100, 123, 456);
    memset(g_Video.pages[1], 9, VIDEO_PAGE_SIZE);
    Blit_RollPutTiny(&pattern, 3, 2);
    CHECK_EQ_INT(g_Video.pages[1][2 * 640 + 3], 0);
    CHECK_EQ_INT(g_Video.pages[1][2 * 640 + 4], 0);
    CHECK_EQ_INT(g_Video.pages[1][2 * 640 + 5], 9);
    CHECK_EQ_INT(g_Video.pages[0][2 * 640 + 3], 0);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.first_lines, 100);
    CHECK_EQ_INT(g_Video.first_address, 123);
    CHECK_EQ_INT(g_Video.second_address, 456);
    Arena_Free(&arena);
}

TEST(cursor_wrap_includes_horizontal_byte_carry)
{
    ARENA arena;
    M_Init(&arena);
    uint8_t data[160] = { 0x80, 2 };
    memset(data + 2, 255, 32);
    const PATTERN pattern = { .size = 0x0210,
                              .data = data,
                              .storage_size = sizeof(data) };
    Blit_RollPutTiny(&pattern, 640, 384);
    CHECK_EQ_INT(g_Video.pages[0][385 * 640], 2);
    CHECK_EQ_INT(g_Video.pages[0][399 * 640 + 15], 2);
    CHECK_EQ_INT(g_Video.pages[0][0], 2);
    CHECK_EQ_INT(g_Video.pages[0][VIDEO_VISIBLE_SIZE], 0);
    memset(g_Video.pages[0], 0, VIDEO_PAGE_SIZE);
    Blit_RollPutTiny(&pattern, 639, 399);
    CHECK_EQ_INT(g_Video.pages[0][VIDEO_VISIBLE_SIZE - 1], 2);
    CHECK_EQ_INT(g_Video.pages[0][VIDEO_VISIBLE_SIZE + 14], 2);
    CHECK_EQ_INT(g_Video.pages[0][639], 2);
    CHECK_EQ_INT(g_Video.pages[0][654], 2);
    CHECK_EQ_INT(g_Video.pages[0][0], 0);
    Arena_Free(&arena);
}

TEST(empty_tiny_sprite_keeps_full_pages_and_state)
{
    ARENA arena;
    M_Init(&arena);
    uint8_t data[2] = {};
    const PATTERN pattern = { .size = 0x0210,
                              .data = data,
                              .storage_size = sizeof(data) };
    memset(g_Video.pages[0], 5, VIDEO_PAGE_SIZE);
    memset(g_Video.pages[1], 9, VIDEO_PAGE_SIZE);
    Video_AccessPage(1);
    const VIDEO before = g_Video;
    Blit_RollPutTiny(&pattern, -1, 399);
    CHECK(memcmp(&g_Video, &before, sizeof(g_Video)) == 0);
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        CHECK_EQ_INT(g_Video.pages[0][pixel], 5);
        CHECK_EQ_INT(g_Video.pages[1][pixel], 9);
    }
    Arena_Free(&arena);
}

TEST(converted_tiny_and_planar_sprites_agree_at_normal_positions)
{
    ARENA arena;
    M_Init(&arena);
    PATTERN_BANK bank;
    PatternBank_Init(&bank, &g_HeapAllocator);
    uint8_t pixels[256];
    const uint8_t colors[] = { 0, 1, 3, 7, 15 };
    for (size_t pixel = 0; pixel < sizeof(pixels); pixel++) {
        pixels[pixel] = colors[(pixel + pixel / 16) % 5];
    }
    uint8_t *const expected = Mem_Alloc(VIDEO_PAGE_SIZE);
    const int16_t tops[] = { 0, 384, 385, 399 };
    for (int16_t left = 0; left < 16; left++) {
        for (size_t top = 0; top < sizeof(tops) / sizeof(tops[0]); top++) {
            CHECK(Result_Ignore(PatternBank_Put(&bank, 0, 16, 16, pixels, 0)));
            memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
            Blit_RollPut(&bank.entries[0], left, tops[top]);
            memcpy(expected, g_Video.pages[0], VIDEO_PAGE_SIZE);
            CHECK(Result_Ignore(PatternBank_ConvertTiny(&bank, 0)));
            memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
            Blit_RollPutTiny(&bank.entries[0], left, tops[top]);
            CHECK(memcmp(g_Video.pages[0], expected, VIDEO_PAGE_SIZE) == 0);
        }
    }
    Mem_Free(expected);
    PatternBank_Reset(&bank);
    Arena_Free(&arena);
}
