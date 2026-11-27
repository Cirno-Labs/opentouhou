#include "tests/test.h"
#include "th02/draw/private.h"
#include "th02/draw/sprites.h"

#include "core/base/arena.h"
#include "core/base/memory.h"

#include <string.h>

static void M_Init(ARENA *const arena, PATTERN_BANK *const bank)
{
    Arena_Init(arena, "sprite test", 1024 * 1024);
    Video_Init(Arena_Allocator(arena));
    PatternBank_Init(bank, &g_HeapAllocator);
}

TEST(planar_sprites_preserve_transparency_and_select_only_the_drawing_page)
{
    ARENA arena;
    PATTERN_BANK bank;
    M_Init(&arena, &bank);
    uint8_t pixels[32];
    for (size_t i = 0; i < sizeof(pixels); i++) {
        pixels[i] = i % 16;
    }
    CHECK(Result_Ignore(PatternBank_Put(&bank, 0, 16, 2, pixels, 0)));
    Video_AccessPage(1);
    Video_ShowPage(0);
    Video_SetSplit(100, 123, 456);
    for (int16_t left = 0; left < 16; left++) {
        memset(g_Video.pages[1], 9, VIDEO_PAGE_SIZE);
        Blit_Put(&bank.entries[0], left, 10);
        for (size_t y = 0; y < 2; y++) {
            for (size_t x = 0; x < 16; x++) {
                CHECK_EQ_INT(
                    g_Video.pages[1][(10 + y) * 640 + left + x],
                    pixels[y * 16 + x] != 0 ? pixels[y * 16 + x] : 9);
            }
        }
        CHECK_EQ_INT(g_Video.pages[1][10 * 640 + left + 16], 9);
        CHECK_EQ_INT(g_Video.pages[0][10 * 640 + left + 1], 0);
    }
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.first_lines, 100);
    CHECK_EQ_INT(g_Video.first_address, 123);
    CHECK_EQ_INT(g_Video.second_address, 456);
    PatternBank_Reset(&bank);
    Arena_Free(&arena);
}

TEST(raw_color_bits_outside_coverage_are_ored_instead_of_discarded)
{
    ARENA arena;
    PATTERN_BANK bank;
    M_Init(&arena, &bank);
    const uint8_t pixels[16] = {};
    CHECK(Result_Ignore(PatternBank_Put(&bank, 0, 8, 2, pixels, 0)));
    uint8_t *const data = bank.entries[0].data;
    data[0] = 0x80;
    data[2] = 0x40;
    data[4] = 0x80;
    data[8] = 0x20;
    memset(g_Video.pages[0], 4, VIDEO_PAGE_SIZE);
    Blit_Put(&bank.entries[0], 0, 0);
    CHECK_EQ_INT(g_Video.pages[0][0], 2);
    CHECK_EQ_INT(g_Video.pages[0][1], 5);
    CHECK_EQ_INT(g_Video.pages[0][2], 12);
    CHECK_EQ_INT(g_Video.pages[0][3], 4);
    PatternBank_Reset(&bank);
    Arena_Free(&arena);
}

TEST(horizontal_overshoot_reaches_the_next_row_and_hidden_vram)
{
    ARENA arena;
    PATTERN_BANK bank;
    M_Init(&arena, &bank);
    uint8_t pixels[16];
    memset(pixels, 15, sizeof(pixels));
    CHECK(Result_Ignore(PatternBank_Put(&bank, 0, 8, 2, pixels, 0)));
    Blit_Put(&bank.entries[0], 639, 399);
    for (size_t i = VIDEO_VISIBLE_SIZE - 1; i < VIDEO_VISIBLE_SIZE + 7; i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], 15);
    }
    for (size_t i = VIDEO_VISIBLE_SIZE + 639; i < VIDEO_VISIBLE_SIZE + 647;
         i++) {
        CHECK_EQ_INT(g_Video.pages[0][i], 15);
    }
    CHECK_EQ_INT(g_Video.pages[0][0], 0);
    PatternBank_Reset(&bank);
    Arena_Free(&arena);
}

TEST(rolling_rows_keep_horizontal_carry_but_restart_after_row_399)
{
    ARENA arena;
    PATTERN_BANK bank;
    M_Init(&arena, &bank);
    uint8_t pixels[16];
    memset(pixels, 2, 8);
    memset(pixels + 8, 3, 8);
    CHECK(Result_Ignore(PatternBank_Put(&bank, 0, 8, 2, pixels, 0)));
    Blit_RollPut(&bank.entries[0], 639, 399);
    CHECK_EQ_INT(g_Video.pages[0][VIDEO_VISIBLE_SIZE - 1], 2);
    CHECK_EQ_INT(g_Video.pages[0][VIDEO_VISIBLE_SIZE + 6], 2);
    CHECK_EQ_INT(g_Video.pages[0][639], 3);
    CHECK_EQ_INT(g_Video.pages[0][646], 3);
    CHECK_EQ_INT(g_Video.pages[0][0], 0);
    CHECK_EQ_INT(g_Video.pages[0][VIDEO_VISIBLE_SIZE + 639], 0);
    PatternBank_Reset(&bank);
    Arena_Free(&arena);
}

TEST(rolling_sprites_retain_negative_rows_in_hidden_vram)
{
    ARENA arena;
    PATTERN_BANK bank;
    M_Init(&arena, &bank);
    uint8_t pixels[32];
    for (size_t row = 0; row < 4; row++) {
        memset(pixels + row * 8, (int)row + 2, 8);
    }
    CHECK(Result_Ignore(PatternBank_Put(&bank, 0, 8, 4, pixels, 0)));
    Blit_RollPut(&bank.entries[0], 368, -2);
    const size_t rows[] = {
        (VIDEO_PLANE_SIZE - 160) * 8,
        (VIDEO_PLANE_SIZE - 80) * 8,
        0,
        VIDEO_WIDTH,
    };
    for (size_t row = 0; row < 4; row++) {
        for (size_t x = 0; x < 8; x++) {
            CHECK_EQ_INT(g_Video.pages[0][rows[row] + 368 + x], row + 2);
            CHECK_EQ_INT(g_Video.pages[0][398 * VIDEO_WIDTH + 368 + x], 0);
            CHECK_EQ_INT(g_Video.pages[0][399 * VIDEO_WIDTH + 368 + x], 0);
        }
    }
    PatternBank_Reset(&bank);
    Arena_Free(&arena);
}

TEST(raw_coordinate_bits_follow_unsigned_x_and_wrapped_byte_offsets)
{
    ARENA arena;
    PATTERN_BANK bank;
    M_Init(&arena, &bank);
    uint8_t pixels[16];
    memset(pixels, 7, sizeof(pixels));
    CHECK(Result_Ignore(PatternBank_Put(&bank, 0, 8, 2, pixels, 0)));
    Blit_Put(&bank.entries[0], -1, 0);
    CHECK_EQ_INT(g_Video.pages[0][65535], 7);
    CHECK_EQ_INT(g_Video.pages[0][65542], 7);
    CHECK_EQ_INT(g_Video.pages[0][0], 0);
    memset(g_Video.pages[0], 0, VIDEO_PAGE_SIZE);
    Blit_Put(&bank.entries[0], 0, -1);
    CHECK_EQ_INT(g_Video.pages[0][(VIDEO_PLANE_SIZE - 80) * 8], 7);
    CHECK_EQ_INT(g_Video.pages[0][0], 7);
    PatternBank_Reset(&bank);
    Arena_Free(&arena);
}

TEST(mask_byte_writes_only_set_bits_with_color_masked_to_four_bits)
{
    ARENA arena;
    PATTERN_BANK bank;
    M_Init(&arena, &bank);
    memset(g_Video.pages[0], 9, 8);
    Draw_MaskByte(0, 0xaa, 0xf3);
    for (size_t bit = 0; bit < 8; bit++) {
        CHECK_EQ_INT(g_Video.pages[0][bit], (bit % 2 == 0) ? 3 : 9);
    }
    Draw_MaskByte(0x8000, 0xff, 5);
    CHECK_EQ_INT(g_Video.pages[0][0], 5);
    PatternBank_Reset(&bank);
    Arena_Free(&arena);
}
