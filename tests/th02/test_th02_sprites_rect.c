#include "th02_clip_fixture.h"
#include "th02/draw/private.h"

#include "core/base/arena.h"

#include "test.h"

static void M_Init(ARENA *const arena, PATTERN_BANK *const bank)
{
    Arena_Init(arena, "clipped sprite test", 1024 * 1024);
    Video_Init(Arena_Allocator(arena));
    PatternBank_Init(bank, &g_HeapAllocator);
}

TEST(screen_edges_crop_shifted_pixels_without_touching_hidden_vram)
{
    ARENA arena;
    PATTERN_BANK bank;
    M_Init(&arena, &bank);
    uint8_t pixels[48];
    for (size_t i = 0; i < sizeof(pixels); i++) {
        pixels[i] = (i * 7) & 15;
    }
    CHECK(Result_Ignore(PatternBank_Put(&bank, 0, 16, 3, pixels, 0)));

    Clip_Reset();
    const int16_t lefts[] = { -17, -16, -15, -8,  -1,  0,  1,
                              7,   8,   15,  630, 639, 640 };
    const int16_t tops[] = { -3, -1, 0, 398, 399, 400 };
    for (size_t xi = 0; xi < sizeof(lefts) / sizeof(lefts[0]); xi++) {
        for (size_t yi = 0; yi < sizeof(tops) / sizeof(tops[0]); yi++) {
            memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
            Blit_PutRect(&bank.entries[0], g_Clip, lefts[xi], tops[yi]);
            for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
                uint8_t expected = 9;
                const int32_t x = (int32_t)(pixel % VIDEO_WIDTH) - lefts[xi];
                const int32_t y = (int32_t)(pixel / VIDEO_WIDTH) - tops[yi];
                if (pixel < VIDEO_VISIBLE_SIZE && x >= 0 && x < 16 && y >= 0
                    && y < 3 && pixels[y * 16 + x] != 0) {
                    expected = pixels[y * 16 + x];
                }
                CHECK_EQ_INT(g_Video.pages[0][pixel], expected);
            }
        }
    }
    PatternBank_Reset(&bank);
    Arena_Free(&arena);
}

TEST(unaligned_narrow_bounds_include_the_right_and_bottom_pixels)
{
    ARENA arena;
    PATTERN_BANK bank;
    M_Init(&arena, &bank);
    uint8_t pixels[96];
    memset(pixels, 7, sizeof(pixels));
    CHECK(Result_Ignore(PatternBank_Put(&bank, 0, 24, 4, pixels, 0)));

    Clip_Reset();
    TestClip_Set(5, 7, 13, 8);
    Blit_PutRect(&bank.entries[0], g_Clip, -2, 6);
    for (size_t pixel = 0; pixel < VIDEO_PAGE_SIZE; pixel++) {
        const size_t x = pixel % VIDEO_WIDTH;
        const size_t y = pixel / VIDEO_WIDTH;
        CHECK_EQ_INT(
            g_Video.pages[0][pixel],
            x >= 5 && x <= 13 && y >= 7 && y <= 8 ? 7 : 0);
    }
    TestClip_Set(639, 399, 639, 399);
    Blit_PutRect(&bank.entries[0], g_Clip, 630, 398);
    CHECK_EQ_INT(g_Video.pages[0][VIDEO_VISIBLE_SIZE - 1], 7);
    CHECK_EQ_INT(g_Video.pages[0][VIDEO_VISIBLE_SIZE - 2], 0);
    CHECK_EQ_INT(g_Video.pages[0][VIDEO_VISIBLE_SIZE], 0);
    PatternBank_Reset(&bank);
    Arena_Free(&arena);
}

TEST(clipped_color_planes_keep_clear_and_or_semantics_and_video_state)
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

    Clip_Reset();
    TestClip_Set(1, 0, 2, 0);
    const CLIP before = *g_Clip;
    Video_AccessPage(1);
    Video_SetSplit(77, 123, 456);
    memset(g_Video.pages[1], 4, VIDEO_PAGE_SIZE);
    Blit_PutRect(&bank.entries[0], g_Clip, 0, 0);
    CHECK_EQ_INT(g_Video.pages[1][0], 4);
    CHECK_EQ_INT(g_Video.pages[1][1], 5);
    CHECK_EQ_INT(g_Video.pages[1][2], 12);
    CHECK_EQ_INT(g_Video.pages[1][3], 4);
    CHECK_EQ_INT(g_Video.pages[0][1], 0);
    CHECK(memcmp(g_Clip, &before, sizeof(before)) == 0);
    CHECK_EQ_INT(g_Video.drawing_page, 1);
    CHECK_EQ_INT(g_Video.visible_page, 0);
    CHECK_EQ_INT(g_Video.first_lines, 77);
    CHECK_EQ_INT(g_Video.first_address, 123);
    CHECK_EQ_INT(g_Video.second_address, 456);
    PatternBank_Reset(&bank);
    Arena_Free(&arena);
}

TEST(signed_coordinate_overflow_preserves_the_unclipped_shortcut)
{
    ARENA arena;
    PATTERN_BANK bank;
    M_Init(&arena, &bank);
    uint8_t pixels[48];
    memset(pixels, 7, sizeof(pixels));
    CHECK(Result_Ignore(PatternBank_Put(&bank, 0, 16, 3, pixels, 0)));

    Clip_Reset();
    uint8_t *const expected = Mem_Alloc(VIDEO_PAGE_SIZE);
    const int16_t positions[][2] = { { INT16_MAX, 0 }, { 0, INT16_MAX } };
    for (size_t i = 0; i < 2; i++) {
        memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
        Blit_Put(&bank.entries[0], positions[i][0], positions[i][1]);
        memcpy(expected, g_Video.pages[0], VIDEO_PAGE_SIZE);
        memset(g_Video.pages[0], 9, VIDEO_PAGE_SIZE);
        Blit_PutRect(
            &bank.entries[0], g_Clip, positions[i][0], positions[i][1]);
        CHECK(memcmp(g_Video.pages[0], expected, VIDEO_PAGE_SIZE) == 0);
    }
    Mem_Free(expected);
    PatternBank_Reset(&bank);
    Arena_Free(&arena);
}
