#include "tests/test.h"
#include "th01/tram.h"

#include "core/base/arena.h"

#include <limits.h>
#include <string.h>

static void M_Init(ARENA *const arena)
{
    Arena_Init(arena, "tram test", 2 * 1024 * 1024);
    Tram_Init(Arena_Allocator(arena));
}

static uint8_t M_Pixel(const int32_t x, const int32_t y)
{
    return g_Tram->pixels[y * VIDEO_WIDTH + x];
}

static int32_t M_Count(const uint8_t index)
{
    int32_t count = 0;
    for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        count += g_Tram->pixels[i] == index;
    }
    return count;
}

TEST(init_allocates_a_clear_visible_layer)
{
    ARENA arena;
    M_Init(&arena);
    CHECK(g_Tram->visible);
    CHECK_EQ_INT(arena.used, VIDEO_PAGE_SIZE);
    CHECK_EQ_INT(M_Count(0), VIDEO_PAGE_SIZE);
    Tram_Fill((GFX_RECT) { 0, 0, 640, 400 }, TRAM_RED);
    Tram_SetVisible(false);
    CHECK_EQ_INT(M_Count(3), VIDEO_PAGE_SIZE);
    Arena_Reset(&arena);
    Tram_Init(Arena_Allocator(&arena));
    CHECK(g_Tram->visible);
    CHECK_EQ_INT(M_Count(0), VIDEO_PAGE_SIZE);
    CHECK_EQ_INT(arena.used, VIDEO_PAGE_SIZE);
    Arena_Free(&arena);
}

TEST(fixed_palette_separates_transparency_and_opaque_black)
{
    CHECK_EQ_INT(g_TramPalette[0].red, 0);
    CHECK_EQ_INT(g_TramPalette[0].green, 0);
    CHECK_EQ_INT(g_TramPalette[0].blue, 0);
    CHECK_EQ_INT(g_TramPalette[0].alpha, 0);
    for (int32_t color = 0; color < 8; color++) {
        const GFX_COLOR actual = g_TramPalette[color + 1];
        CHECK_EQ_INT(actual.red, (color & 2) != 0 ? 255 : 0);
        CHECK_EQ_INT(actual.green, (color & 4) != 0 ? 255 : 0);
        CHECK_EQ_INT(actual.blue, (color & 1) != 0 ? 255 : 0);
        CHECK_EQ_INT(actual.alpha, 255);
    }
}

TEST(hide_show_and_clear_preserve_the_other_state)
{
    ARENA arena;
    M_Init(&arena);
    Tram_Fill((GFX_RECT) { 0, 0, 1, 1 }, TRAM_WHITE);
    Tram_SetVisible(false);
    CHECK(!g_Tram->visible);
    CHECK_EQ_INT(M_Pixel(0, 0), 8);
    Tram_Clear();
    CHECK(!g_Tram->visible);
    CHECK_EQ_INT(M_Count(0), VIDEO_PAGE_SIZE);
    Tram_Fill((GFX_RECT) { 639, 399, 1, 1 }, TRAM_BLACK);
    Tram_SetVisible(true);
    CHECK(g_Tram->visible);
    CHECK_EQ_INT(M_Pixel(639, 399), 1);
    Arena_Free(&arena);
}

TEST(fill_uses_half_open_bounds_and_all_color_indices)
{
    ARENA arena;
    M_Init(&arena);
    for (int32_t color = -1; color < 8; color++) {
        Tram_Fill((GFX_RECT) { (color + 1) * 4, 2, 3, 2 }, (TRAM_COLOR)color);
        for (int32_t y = 2; y < 4; y++) {
            for (int32_t x = 0; x < 3; x++) {
                CHECK_EQ_INT(M_Pixel((color + 1) * 4 + x, y), color + 1);
            }
            CHECK_EQ_INT(M_Pixel((color + 1) * 4 + 3, y), 0);
        }
    }
    Tram_Fill((GFX_RECT) { -2, -3, 4, 5 }, TRAM_RED);
    CHECK_EQ_INT(M_Pixel(0, 0), 3);
    CHECK_EQ_INT(M_Pixel(1, 1), 3);
    CHECK_EQ_INT(M_Pixel(2, 1), 0);
    Tram_Fill((GFX_RECT) { 638, 398, 20, 20 }, TRAM_GREEN);
    CHECK_EQ_INT(M_Pixel(639, 399), 5);
    const uint8_t last = M_Pixel(639, 399);
    Tram_Fill((GFX_RECT) { 639, 399, 0, 1 }, TRAM_BLUE);
    Tram_Fill((GFX_RECT) { 639, 399, 1, -1 }, TRAM_BLUE);
    CHECK_EQ_INT(M_Pixel(639, 399), last);
    Arena_Free(&arena);
}

TEST(halfwidth_dots_expand_into_exact_sixteen_pixel_blocks)
{
    ARENA arena;
    M_Init(&arena);
    const GAME_TEXT_GLYPH glyph = {
        .codepoint = 'A',
        .rows = { 0x8100, 0x4200 },
        .width = 8,
    };
    const GAME_TEXT_GLYPH before = glyph;
    const GAME_TEXT_GLYPH *const line[] = { &glyph };
    Tram_PutLine16(line, 1, TRAM_RED, TRAM_TRANSPARENT);
    CHECK_EQ_INT(M_Count(3), 4 * 16 * 16);
    for (int32_t y = 80; y < 96; y++) {
        CHECK_EQ_INT(M_Pixel(256, y), 3);
        CHECK_EQ_INT(M_Pixel(271, y), 3);
        CHECK_EQ_INT(M_Pixel(272, y), 0);
        CHECK_EQ_INT(M_Pixel(368, y), 3);
        CHECK_EQ_INT(M_Pixel(383, y), 3);
        CHECK_EQ_INT(M_Pixel(384, y), 0);
    }
    CHECK_EQ_INT(M_Pixel(272, 96), 3);
    CHECK_EQ_INT(M_Pixel(352, 111), 3);
    CHECK_EQ_INT(M_Pixel(256, 112), 0);
    CHECK(memcmp(&glyph, &before, sizeof(glyph)) == 0);
    Arena_Free(&arena);
}

TEST(fullwidth_composition_replaces_foreground_and_background)
{
    ARENA arena;
    M_Init(&arena);
    Tram_Fill((GFX_RECT) { 0, 0, 640, 400 }, TRAM_BLUE);
    const GAME_TEXT_GLYPH glyph = {
        .codepoint = 0x6771,
        .rows = { 0x8001 },
        .width = 16,
    };
    const GAME_TEXT_GLYPH *const line[] = { &glyph };
    Tram_PutLine16(line, 1, TRAM_TRANSPARENT, TRAM_BLACK);
    CHECK_EQ_INT(M_Pixel(192, 80), 0);
    CHECK_EQ_INT(M_Pixel(207, 95), 0);
    CHECK_EQ_INT(M_Pixel(208, 80), 1);
    CHECK_EQ_INT(M_Pixel(431, 80), 1);
    CHECK_EQ_INT(M_Pixel(432, 80), 0);
    CHECK_EQ_INT(M_Pixel(447, 95), 0);
    CHECK_EQ_INT(M_Pixel(192, 96), 1);
    CHECK_EQ_INT(M_Pixel(191, 80), 1);
    CHECK_EQ_INT(M_Pixel(448, 80), 1);
    CHECK_EQ_INT(M_Pixel(0, 336), 2);
    Arena_Free(&arena);
}

TEST(five_letters_fill_the_source_stage_band_only)
{
    ARENA arena;
    M_Init(&arena);
    Tram_Fill((GFX_RECT) { 0, 0, 640, 400 }, TRAM_GREEN);
    const GAME_TEXT_GLYPH glyph = { .codepoint = 'S',
                                    .rows = { 0x8000 },
                                    .width = 8 };
    const GAME_TEXT_GLYPH *const letters[] = { &glyph, &glyph, &glyph, &glyph,
                                               &glyph };
    Tram_PutLine16(letters, 5, TRAM_RED, TRAM_TRANSPARENT);
    CHECK_EQ_INT(M_Count(3), 5 * 16 * 16);
    for (int32_t letter = 0; letter < 5; letter++) {
        CHECK_EQ_INT(M_Pixel(letter * 128, 80), 3);
        CHECK_EQ_INT(M_Pixel(letter * 128 + 15, 95), 3);
        CHECK_EQ_INT(M_Pixel(letter * 128 + 16, 80), 0);
    }
    CHECK_EQ_INT(M_Pixel(0, 79), 0);
    CHECK_EQ_INT(M_Pixel(639, 335), 0);
    CHECK_EQ_INT(M_Pixel(0, 336), 5);
    CHECK_EQ_INT(M_Pixel(639, 399), 5);
    Tram_Fill((GFX_RECT) { 0, 336, 640, 80 }, TRAM_TRANSPARENT);
    CHECK_EQ_INT(M_Pixel(639, 399), 0);
    Arena_Free(&arena);
}

TEST(two_digits_and_up_are_centered_without_clearing_lower_rows)
{
    ARENA arena;
    M_Init(&arena);
    Tram_Fill((GFX_RECT) { 0, 336, 640, 64 }, TRAM_WHITE);
    const GAME_TEXT_GLYPH glyph = { .codepoint = '0',
                                    .rows = { 0x8100 },
                                    .width = 8 };
    const GAME_TEXT_GLYPH *const digits[] = { &glyph, &glyph };
    Tram_PutLine16(digits, 2, TRAM_RED, TRAM_TRANSPARENT);
    CHECK_EQ_INT(M_Pixel(191, 80), 0);
    CHECK_EQ_INT(M_Pixel(192, 80), 3);
    CHECK_EQ_INT(M_Pixel(319, 95), 3);
    CHECK_EQ_INT(M_Pixel(320, 80), 3);
    CHECK_EQ_INT(M_Pixel(447, 95), 3);
    CHECK_EQ_INT(M_Pixel(448, 80), 0);
    CHECK_EQ_INT(M_Pixel(639, 399), 8);
    Arena_Free(&arena);
}

TEST(reverse_kanji_covers_screen_except_centered_glyph_holes)
{
    ARENA arena;
    M_Init(&arena);
    const GAME_TEXT_GLYPH glyph = {
        .codepoint = 0x6771,
        .rows = { 0x8001, 0x0180 },
        .width = 16,
    };
    Tram_SetVisible(false);
    Tram_PutKanji16Reverse(&glyph);
    CHECK(!g_Tram->visible);
    CHECK_EQ_INT(M_Count(0), 4 * 16 * 16);
    CHECK_EQ_INT(M_Count(1), VIDEO_PAGE_SIZE - 4 * 16 * 16);
    CHECK_EQ_INT(M_Pixel(192, 80), 0);
    CHECK_EQ_INT(M_Pixel(447, 95), 0);
    CHECK_EQ_INT(M_Pixel(304, 96), 0);
    CHECK_EQ_INT(M_Pixel(335, 111), 0);
    CHECK_EQ_INT(M_Pixel(0, 0), 1);
    CHECK_EQ_INT(M_Pixel(639, 399), 1);
    Arena_Free(&arena);
}

TEST(extreme_rectangles_stay_inside_storage)
{
    ARENA arena;
    Arena_Init(&arena, "tram guards", 1024 * 1024);
    uint8_t *const first = Arena_Alloc(&arena, 16);
    memset(first, 0xa5, 16);
    Tram_Init(Arena_Allocator(&arena));
    uint8_t *const last = Arena_Alloc(&arena, 16);
    memset(last, 0x5a, 16);
    Tram_Fill((GFX_RECT) { INT32_MIN, 0, INT32_MAX, 400 }, TRAM_WHITE);
    Tram_Fill((GFX_RECT) { INT32_MAX, 0, INT32_MAX, 400 }, TRAM_WHITE);
    for (int32_t i = 0; i < 16; i++) {
        CHECK_EQ_INT(first[i], 0xa5);
        CHECK_EQ_INT(last[i], 0x5a);
    }
    Arena_Free(&arena);
}

TEST(overlay_drawing_leaves_graphics_pages_selectors_and_dac_unchanged)
{
    ARENA arena;
    M_Init(&arena);
    Video_Init(Arena_Allocator(&arena));
    Video_Clear(10);
    Video_SelectDrawingPage(1);
    Video_Clear(7);
    Video_SelectVisiblePage(1);
    Video_SetDac(2, 5, 6, 7);
    const VIDEO saved = *g_Video;
    const size_t used = arena.used;
    const GAME_TEXT_GLYPH glyph = { .codepoint = 0x6771,
                                    .rows = { 0xffff },
                                    .width = 16 };
    Tram_PutKanji16Reverse(&glyph);
    Tram_SetVisible(false);
    Tram_Clear();
    CHECK_EQ_INT(arena.used, used);
    CHECK_EQ_INT(g_Video->drawing_page, saved.drawing_page);
    CHECK_EQ_INT(g_Video->visible_page, saved.visible_page);
    CHECK(
        memcmp(g_Video->palette, saved.palette, sizeof(g_Video->palette)) == 0);
    for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(g_Video->pages[0][i], 10);
        CHECK_EQ_INT(g_Video->pages[1][i], 7);
    }
    Video_SetDac(2, 0, 0, 0);
    CHECK_EQ_INT(g_TramPalette[3].red, 255);
    Arena_Free(&arena);
}

TEST(empty_centered_line_clears_only_its_band)
{
    ARENA arena;
    M_Init(&arena);
    Tram_Fill((GFX_RECT) { 0, 0, 640, 400 }, TRAM_WHITE);
    Tram_PutLine16(nullptr, 0, TRAM_RED, TRAM_TRANSPARENT);
    CHECK_EQ_INT(M_Count(0), 640 * 336);
    CHECK_EQ_INT(M_Count(8), 640 * 64);
    Arena_Free(&arena);
}
