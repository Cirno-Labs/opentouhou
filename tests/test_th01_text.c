#include "th01/text.h"

#include "stg/game_font.h"

#include "core/base/arena.h"
#include "core/text.h"

#include "test.h"
#include "test_fonts.h"

#include <limits.h>

static void M_Init(ARENA *const arena, VIDEO *const video)
{
    Arena_Init(arena, "text test", 1024 * 1024);
    Video_Init(video, Arena_Allocator(arena));
    Video_Clear(video, 3);
}

static GAME_TEXT_FONT M_Font(const ALLOCATOR *const allocator)
{
    const GAME_TEXT_GLYPH glyphs[] = {
        { .codepoint = 0x3042, .rows = { 0x8001, 0x0550 }, .width = 16 },
        { .codepoint = 'A', .rows = { 0x4500, 0x8100 }, .width = 8 },
        { .codepoint = 0x20, .width = 8 },
        { .codepoint = 0xff21, .rows = { 0x8001, 0x0550 }, .width = 16 },
        { .codepoint = 0xff71, .rows = { 0x8100, 0x1800 }, .width = 8 },
        { .codepoint = 0x3000, .width = 16 },
    };
    GAME_TEXT_FONT font;
    CHECK(IS_OK(
        GameText_LoadGlyphs(glyphs, ARRAY_SIZE(glyphs), allocator, &font)));
    return font;
}

static void M_Row(
    const VIDEO *const video, const int32_t page, const int32_t x,
    const int32_t y, const int32_t width, const uint16_t mask,
    const uint8_t color, const uint8_t background)
{
    for (int32_t i = 0; i < width; i++) {
        CHECK_EQ_INT(
            Video_ReadPixel(video, page, x + i, y),
            (mask & (0x8000u >> i)) != 0 ? color : background);
    }
}

TEST(bitmap_loading_sorts_copies_and_preserves_caller_storage)
{
    ARENA arena;
    Arena_Init(&arena, "glyphs", 4096);
    GAME_TEXT_GLYPH glyphs[] = {
        { .codepoint = 0x3042, .rows = { 0x8001 }, .width = 16 },
        { .codepoint = 'A', .rows = { 0x8100 }, .width = 8 },
    };
    GAME_TEXT_FONT font;
    CHECK(
        IS_OK(GameText_LoadGlyphs(glyphs, 2, Arena_Allocator(&arena), &font)));
    CHECK_EQ_INT(font.count, 2);
    CHECK_EQ_INT(font.glyphs[0].codepoint, 'A');
    CHECK_EQ_INT(glyphs[0].codepoint, 0x3042);
    glyphs[1].rows[0] = 0;
    CHECK_EQ_INT(GameText_Find(&font, 'A')->rows[0], 0x8100);
    CHECK_EQ_INT(GameText_Find(&font, 0x3042)->width, 16);
    CHECK(GameText_Find(&font, 'B') == nullptr);
    CHECK((const uint8_t *)font.glyphs >= arena.base);
    CHECK(
        (const uint8_t *)(font.glyphs + font.count) <= arena.base + arena.used);
    Arena_Free(&arena);
}

TEST(invalid_bitmap_records_leave_no_caller_allocations)
{
    ARENA arena;
    Arena_Init(&arena, "invalid glyphs", 4096);
    GAME_TEXT_FONT font;
    GAME_TEXT_GLYPH glyphs[2] = {
        { .codepoint = 'A', .width = 8 },
        { .codepoint = 'A', .width = 8 },
    };
    CHECK(!Result_Ignore(
        GameText_LoadGlyphs(glyphs, 2, Arena_Allocator(&arena), &font)));
    CHECK(font.glyphs == nullptr && font.count == 0);
    CHECK_EQ_INT(arena.used, 0);
    glyphs[0].codepoint = 0xd800;
    CHECK(!Result_Ignore(
        GameText_LoadGlyphs(glyphs, 1, Arena_Allocator(&arena), &font)));
    glyphs[0].codepoint = 0x110000;
    CHECK(!Result_Ignore(
        GameText_LoadGlyphs(glyphs, 1, Arena_Allocator(&arena), &font)));
    glyphs[0].codepoint = 'A';
    glyphs[0].width = 16;
    CHECK(!Result_Ignore(
        GameText_LoadGlyphs(glyphs, 1, Arena_Allocator(&arena), &font)));
    glyphs[0].width = 8;
    glyphs[0].rows[0] = 1;
    CHECK(!Result_Ignore(
        GameText_LoadGlyphs(glyphs, 1, Arena_Allocator(&arena), &font)));
    glyphs[0].rows[0] = 0;
    glyphs[0].codepoint = '\n';
    CHECK(!Result_Ignore(
        GameText_LoadGlyphs(glyphs, 1, Arena_Allocator(&arena), &font)));
    CHECK(!Result_Ignore(
        GameText_LoadGlyphs(nullptr, 1, Arena_Allocator(&arena), &font)));
    CHECK(!Result_Ignore(
        GameText_LoadGlyphs(glyphs, -1, Arena_Allocator(&arena), &font)));
    CHECK(
        IS_OK(GameText_LoadGlyphs(nullptr, 0, Arena_Allocator(&arena), &font)));
    CHECK(font.glyphs == nullptr && font.count == 0);
    CHECK_EQ_INT(arena.used, 0);
    Arena_Free(&arena);
}

TEST(measurement_uses_source_cells_and_excludes_the_final_gap)
{
    ARENA arena;
    Arena_Init(&arena, "measure glyphs", 4096);
    const GAME_TEXT_FONT font = M_Font(Arena_Allocator(&arena));
    GAME_TEXT_STYLE style = {};
    int32_t width;
    for (int32_t spacing = 0; spacing <= 7; spacing++) {
        style.spacing = spacing;
        CHECK(IS_OK(GameText_Measure(&font, "AあＡｱ　", &style, &width)));
        CHECK_EQ_INT(width, 64 + 4 * spacing);
        CHECK(IS_OK(GameText_Measure(&font, "", &style, &width)));
        CHECK_EQ_INT(width, 0);
        CHECK(IS_OK(GameText_Measure(&font, "A", &style, &width)));
        CHECK_EQ_INT(width, 8);
    }
    Arena_Free(&arena);
}

TEST(controls_map_to_halfwidth_space_and_unknown_scalars_keep_their_cells)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    const GAME_TEXT_FONT font = M_Font(Arena_Allocator(&arena));
    const GAME_TEXT_STYLE style = {};
    int32_t width;
    CHECK(IS_OK(GameText_Measure(
        &font,
        "A\n\t\x7f\xc2\x85"
        "B東",
        &style, &width)));
    CHECK_EQ_INT(width, 64);
    CHECK(GameText_Find(&font, '\n') == GameText_Find(&font, ' '));
    CHECK(IS_OK(GameText_Draw(&font, &video, "\nB東", 10, 10, 10, &style)));
    for (int32_t y = 10; y < 26; y++) {
        for (int32_t x = 10; x < 42; x++) {
            CHECK_EQ_INT(Video_ReadPixel(&video, 0, x, y), 3);
        }
    }
    Arena_Free(&arena);
}

TEST(weights_clip_left_overflow_and_preserve_source_holes)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    const GAME_TEXT_FONT font = M_Font(Arena_Allocator(&arena));
    const uint16_t masks[] = { 0x4500, 0xcf00, 0xcb00, 0xdf00 };
    const uint16_t second[] = { 0x8100, 0x8300, 0x8300, 0x8700 };
    for (int32_t weight = GAME_TEXT_NORMAL; weight <= GAME_TEXT_BLACK;
         weight++) {
        GAME_TEXT_STYLE style = { .weight = weight };
        CHECK(IS_OK(GameText_Draw(
            &font, &video, "A", 10, 10 + weight * 20, 10, &style)));
        M_Row(&video, 0, 10, 10 + weight * 20, 8, masks[weight], 10, 3);
        M_Row(&video, 0, 10, 11 + weight * 20, 8, second[weight], 10, 3);
        CHECK_EQ_INT(Video_ReadPixel(&video, 0, 9, 10 + weight * 20), 3);
        CHECK_EQ_INT(Video_ReadPixel(&video, 0, 18, 10 + weight * 20), 3);
    }
    Arena_Free(&arena);
}

TEST(reverse_follows_weight_and_uses_the_full_or_half_cell_mask)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    const GAME_TEXT_FONT font = M_Font(Arena_Allocator(&arena));
    const GAME_TEXT_STYLE style = { .weight = GAME_TEXT_BOLD, .reverse = true };
    CHECK(IS_OK(GameText_Draw(&font, &video, "Aあ", 10, 10, 26, &style)));
    M_Row(&video, 0, 10, 10, 8, 0x3400, 10, 3);
    M_Row(&video, 0, 18, 10, 16, 0x7ffc, 10, 3);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 34, 10), 3);
    Arena_Free(&arena);
}

TEST(fullwidth_and_halfwidth_glyphs_advance_with_spacing)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    const GAME_TEXT_FONT font = M_Font(Arena_Allocator(&arena));
    const GAME_TEXT_STYLE style = { .spacing = 3 };
    CHECK(IS_OK(GameText_Draw(&font, &video, "AあｱＡ", 10, 10, 5, &style)));
    M_Row(&video, 0, 10, 10, 8, 0x4500, 5, 3);
    M_Row(&video, 0, 21, 10, 16, 0x8001, 5, 3);
    M_Row(&video, 0, 40, 10, 8, 0x8100, 5, 3);
    M_Row(&video, 0, 51, 10, 16, 0x8001, 5, 3);
    for (int32_t x = 18; x < 21; x++) {
        CHECK_EQ_INT(Video_ReadPixel(&video, 0, x, 10), 3);
    }
    Arena_Free(&arena);
}

TEST(background_and_underline_cover_the_measured_extent_only)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    const GAME_TEXT_FONT font = M_Font(Arena_Allocator(&arena));
    const GAME_TEXT_STYLE style = { .spacing = 2,
                                    .clear_background = true,
                                    .underline = true };
    CHECK(IS_OK(GameText_Draw(&font, &video, "Aあ", 10, 10, 10, &style)));
    for (int32_t y = 10; y <= 27; y++) {
        for (int32_t x = 10; x < 36; x++) {
            const uint8_t pixel = Video_ReadPixel(&video, 0, x, y);
            if (y == 27) {
                CHECK_EQ_INT(pixel, 7);
            } else if (y >= 12) {
                CHECK_EQ_INT(pixel, 0);
            } else {
                CHECK(pixel == 0 || pixel == 10);
            }
        }
        CHECK_EQ_INT(Video_ReadPixel(&video, 0, 9, y), 3);
        CHECK_EQ_INT(Video_ReadPixel(&video, 0, 36, y), 3);
    }
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 10, 28), 3);
    Arena_Free(&arena);
}

TEST(underline_without_background_keeps_the_gap_and_uncovered_pixels)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    const GAME_TEXT_FONT font = M_Font(Arena_Allocator(&arena));
    const GAME_TEXT_STYLE style = { .underline = true };
    CHECK(IS_OK(GameText_Draw(&font, &video, "A", 10, 10, 5, &style)));
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 10, 26), 3);
    for (int32_t x = 10; x < 18; x++) {
        CHECK_EQ_INT(Video_ReadPixel(&video, 0, x, 27), 7);
    }
    Arena_Free(&arena);
}

TEST(edge_clipping_is_safe_and_right_edge_requires_a_complete_cell)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    const GAME_TEXT_FONT font = M_Font(Arena_Allocator(&arena));
    const GAME_TEXT_STYLE style = {};
    CHECK(IS_OK(GameText_Draw(&font, &video, "A", -7, -1, 10, &style)));
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 0, 0), 10);
    CHECK(IS_OK(GameText_Draw(&font, &video, "A", 632, 390, 10, &style)));
    M_Row(&video, 0, 632, 390, 8, 0x4500, 10, 3);
    CHECK(IS_OK(GameText_Draw(&font, &video, "A", 633, 370, 10, &style)));
    CHECK(IS_OK(GameText_Draw(&font, &video, "あ", 625, 350, 10, &style)));
    for (int32_t x = 625; x < 640; x++) {
        CHECK_EQ_INT(Video_ReadPixel(&video, 0, x, 350), 3);
        CHECK_EQ_INT(Video_ReadPixel(&video, 0, x, 370), 3);
    }
    const GAME_TEXT_STYLE background = { .clear_background = true,
                                         .underline = true };
    CHECK(IS_OK(GameText_Draw(
        &font, &video, "A", INT32_MAX, INT32_MAX, 1, &background)));
    CHECK(IS_OK(GameText_Draw(
        &font, &video, "A", INT32_MIN, INT32_MIN, 1, &background)));
    Arena_Free(&arena);
}

TEST(drawing_preserves_other_page_selectors_palette_and_cache)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    Video_SelectDrawingPage(&video, 1);
    Video_SelectVisiblePage(&video, 0);
    Video_Clear(&video, 4);
    Video_SetDac(&video, 0, 3, 7, 10);
    GFX_COLOR palette[16];
    memcpy(palette, video.palette, sizeof(palette));
    const GAME_TEXT_FONT font = M_Font(Arena_Allocator(&arena));
    const GAME_TEXT_GLYPH before = *GameText_Find(&font, 'A');
    const size_t used = arena.used;
    const GAME_TEXT_STYLE style = {};
    CHECK(IS_OK(GameText_Draw(&font, &video, "A", 10, 10, 5, &style)));
    M_Row(&video, 1, 10, 10, 8, 0x4500, 5, 4);
    CHECK_EQ_INT(Video_ReadPixel(&video, 0, 10, 10), 3);
    CHECK_EQ_INT(video.drawing_page, 1);
    CHECK_EQ_INT(video.visible_page, 0);
    CHECK(memcmp(palette, video.palette, sizeof(palette)) == 0);
    CHECK(memcmp(&before, GameText_Find(&font, 'A'), sizeof(before)) == 0);
    CHECK_EQ_INT(arena.used, used);
    Arena_Free(&arena);
}

TEST(malformed_utf8_and_invalid_styles_leave_video_unchanged)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    const GAME_TEXT_FONT font = M_Font(Arena_Allocator(&arena));
    GAME_TEXT_STYLE style = { .clear_background = true, .underline = true };
    static const char *const invalid[] = {
        "A\xc0\xaf",
        "A\xe0\x80\xaf",
        "A\xed\xa0\x80",
        "A\xf0\x80\x80\xaf",
        "A\xf4\x90\x80\x80",
        "A\xe3\x81",
        "A\x80",
        "A\xff",
    };
    for (size_t i = 0; i < ARRAY_SIZE(invalid); i++) {
        int32_t width = 123;
        CHECK(!Result_Ignore(
            GameText_Measure(&font, invalid[i], &style, &width)));
        CHECK_EQ_INT(width, 0);
        CHECK(!Result_Ignore(
            GameText_Draw(&font, &video, invalid[i], 10, 10, 5, &style)));
    }
    style.spacing = 8;
    CHECK(!Result_Ignore(GameText_Draw(&font, &video, "A", 10, 10, 5, &style)));
    style.spacing = -1;
    CHECK(!Result_Ignore(GameText_Draw(&font, &video, "A", 10, 10, 5, &style)));
    style.spacing = 0;
    style.weight = (GAME_TEXT_WEIGHT)99;
    CHECK(!Result_Ignore(GameText_Draw(&font, &video, "A", 10, 10, 5, &style)));
    CHECK(
        !Result_Ignore(GameText_Draw(&font, &video, "A", 10, 10, 5, nullptr)));
    for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(video.pages[0][i], 3);
    }
    Arena_Free(&arena);
}

TEST(empty_text_is_a_noop_with_every_background_effect)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    const GAME_TEXT_FONT font = {};
    const GAME_TEXT_STYLE style = {
        .weight = GAME_TEXT_BLACK,
        .spacing = 7,
        .clear_background = true,
        .underline = true,
        .reverse = true,
    };
    CHECK(IS_OK(GameText_Draw(&font, &video, "", 10, 10, 10, &style)));
    for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        CHECK_EQ_INT(video.pages[0][i], 3);
    }
    Arena_Free(&arena);
}

TEST(core_font_cache_is_unique_and_survives_font_and_temporary_storage)
{
    CHECK(IS_OK(Text_Init()));
    FONT *source = nullptr;
    TestFonts_UseBuiltin();
    CHECK(IS_OK(GameFont_Open(nullptr, 16, &source)));
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    GAME_TEXT_FONT font;
    CHECK(IS_OK(
        GameText_Load(source, "AあＡｱ　 A\n", Arena_Allocator(&arena), &font)));
    CHECK_EQ_INT(font.count, 6);
    const uint32_t points[] = { 'A', 0x3042, 0xff21, 0xff71 };
    for (size_t i = 0; i < ARRAY_SIZE(points); i++) {
        const GAME_TEXT_GLYPH *const glyph = GameText_Find(&font, points[i]);
        CHECK(glyph != nullptr);
        bool visible = false;
        for (int32_t y = 0; y < 16; y++) {
            visible |= glyph->rows[y] != 0;
            if (glyph->width == 8) {
                CHECK_EQ_INT(glyph->rows[y] & 0xff, 0);
            }
        }
        CHECK(visible);
    }
    CHECK_EQ_INT(GameText_Find(&font, 'A')->width, 8);
    CHECK_EQ_INT(GameText_Find(&font, 0xff21)->width, 16);
    for (int32_t y = 0; y < 16; y++) {
        CHECK_EQ_INT(GameText_Find(&font, ' ')->rows[y], 0);
        CHECK_EQ_INT(GameText_Find(&font, 0x3000)->rows[y], 0);
    }
    Font_Free(source);
    Text_Shutdown();
    const size_t used = arena.used;
    const GAME_TEXT_STYLE style = { .weight = GAME_TEXT_BOLD };
    CHECK(IS_OK(GameText_Draw(&font, &video, "AあＡｱ", 10, 10, 10, &style)));
    CHECK_EQ_INT(arena.used, used);
    bool drawn = false;
    for (int32_t i = 0; i < VIDEO_PAGE_SIZE; i++) {
        drawn |= video.pages[0][i] == 10;
    }
    CHECK(drawn);
    Arena_Free(&arena);
}

TEST(core_font_load_validation_does_not_allocate_from_the_caller)
{
    ARENA arena;
    Arena_Init(&arena, "failed font", 4096);
    GAME_TEXT_FONT font;
    CHECK(!Result_Ignore(
        GameText_Load(nullptr, "A", Arena_Allocator(&arena), &font)));
    CHECK(font.glyphs == nullptr && font.count == 0);
    CHECK_EQ_INT(arena.used, 0);
    CHECK(IS_OK(Text_Init()));
    FONT *source = nullptr;
    TestFonts_UseBuiltin();
    CHECK(IS_OK(GameFont_Open(nullptr, 16, &source)));
    CHECK(!Result_Ignore(
        GameText_Load(source, "A\xe3\x81", Arena_Allocator(&arena), &font)));
    CHECK(font.glyphs == nullptr && font.count == 0);
    CHECK_EQ_INT(arena.used, 0);
    CHECK(IS_OK(GameText_Load(source, "", Arena_Allocator(&arena), &font)));
    CHECK(font.glyphs == nullptr && font.count == 0);
    CHECK_EQ_INT(arena.used, 0);
    Font_Free(source);
    Text_Shutdown();
    Arena_Free(&arena);
}

TEST(utf8_scalars_include_two_and_four_byte_characters)
{
    ARENA arena;
    VIDEO video;
    M_Init(&arena, &video);
    const GAME_TEXT_GLYPH glyphs[] = {
        { .codepoint = 0xa3, .width = 16, .rows = { 0x8001 } },
        { .codepoint = 0x1f600, .width = 16, .rows = { 0x4002 } },
        { .codepoint = 0x10ffff, .width = 16, .rows = { 0x2004 } },
    };
    GAME_TEXT_FONT font;
    CHECK(
        IS_OK(GameText_LoadGlyphs(glyphs, 3, Arena_Allocator(&arena), &font)));
    const GAME_TEXT_STYLE style = {};
    const char text[] = "\xc2\xa3\xf0\x9f\x98\x80\xf4\x8f\xbf\xbf";
    int32_t width;
    CHECK(IS_OK(GameText_Measure(&font, text, &style, &width)));
    CHECK_EQ_INT(width, 48);
    CHECK(IS_OK(GameText_Draw(&font, &video, text, 10, 10, 10, &style)));
    M_Row(&video, 0, 10, 10, 16, 0x8001, 10, 3);
    M_Row(&video, 0, 26, 10, 16, 0x4002, 10, 3);
    M_Row(&video, 0, 42, 10, 16, 0x2004, 10, 3);
    Arena_Free(&arena);
}

TEST(core_font_rasterization_obeys_the_fixed_cell_sampling_contract)
{
    CHECK(IS_OK(Text_Init()));
    FONT *source = nullptr;
    TestFonts_UseBuiltin();
    CHECK(IS_OK(GameFont_Open(nullptr, 23, &source)));
    ARENA arena;
    Arena_Init(&arena, "sampled glyphs", 4096);
    GAME_TEXT_FONT font;
    CHECK(IS_OK(GameText_Load(source, "Aあ", Arena_Allocator(&arena), &font)));
    const char *const strings[] = { "A", "あ" };
    const uint32_t points[] = { 'A', 0x3042 };
    const TEXT_STYLE style = TextStyle_Default();
    for (size_t i = 0; i < ARRAY_SIZE(strings); i++) {
        IMAGE image;
        CHECK(IS_OK(
            Text_Render(source, strings[i], &style, &g_HeapAllocator, &image)));
        const GAME_TEXT_GLYPH *const glyph = GameText_Find(&font, points[i]);
        CHECK(image.height != 16);
        CHECK(glyph != nullptr);
        for (int32_t y = 0; y < 16; y++) {
            for (int32_t x = 0; x < glyph->width; x++) {
                const int32_t sx = x * image.width / glyph->width;
                const int32_t sy = y * image.height / 16;
                const bool covered =
                    image.pixels
                        [((size_t)sy * (size_t)image.width + (size_t)sx) * 4
                         + 3]
                    >= 128;
                CHECK(((glyph->rows[y] & (0x8000u >> x)) != 0) == covered);
            }
        }
        Image_Free(&image);
    }
    Font_Free(source);
    Text_Shutdown();
    Arena_Free(&arena);
}
