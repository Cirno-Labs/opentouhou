#include "tests/test.h"
#include "tests/test_fonts.h"

#include "core/patch.h"
#include "core/text.h"
#include "core/text/private.h"
#ifdef TEST_MS_GOTHIC
    #include "tests/patch_builder.h"
#endif

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/text/font_registry.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#ifdef TEST_MS_GOTHIC
    #define M_FAMILY "ＭＳ ゴシック"
#else
    #define M_FAMILY "IPAGothic"
#endif

// Returns the file of the font under test. Mem_Free releases it.
static uint8_t *M_ReadFont(size_t *const size)
{
#ifdef TEST_MS_GOTHIC
    return TestFonts_ReadFile(TestFonts_RequireMsGothic(), size);
#else
    return TestFonts_Read(TEST_FONT_GOTHIC, size);
#endif
}

// Adds data to the registry, where it wins M_FAMILY, and opens its face of
// M_FAMILY. The fonts that the registry had stay open, as their data stays
// until M_Shutdown.
static FONT *M_OpenData(
    const uint8_t *const data, const size_t size, const float pixel_height)
{
    CHECK(IS_OK(FontRegistry_Add("font under test", data, size)));
    FONT *font = nullptr;
    CHECK(IS_OK(FontRegistry_Open(M_FAMILY, pixel_height, &font)));
    return font;
}

// Adds the font under test to the registry, as M_OpenData does, and opens
// it.
static RESULT M_Open(const float pixel_height, FONT **const out)
{
    size_t size;
    uint8_t *const data = M_ReadFont(&size);
    const RESULT added = FontRegistry_Add("font under test", data, size);
    Mem_Free(data);
    MUST(added);
    return FontRegistry_Open(M_FAMILY, pixel_height, out);
}

// Forgets the fonts of the registry and shuts text down, after the test
// has freed its fonts.
static void M_Shutdown(void)
{
    FontRegistry_Reset(false);
    Text_Shutdown();
}

// Returns the table of the first face of the font in data with tag, or
// nullptr.
static uint8_t *M_Table(uint8_t *const data, const char *const tag)
{
    const uint8_t *const directory = memcmp(data, "ttcf", 4) == 0
        ? data + TestFonts_ReadU32Be(data + 12)
        : data;
    const int32_t count = directory[4] << 8 | directory[5];
    for (int32_t i = 0; i < count; i++) {
        const uint8_t *const record = directory + 12 + i * 16;
        if (memcmp(record, tag, 4) == 0) {
            return data + TestFonts_ReadU32Be(record + 8);
        }
    }
    return nullptr;
}

// Returns the units per em of the font under test.
static int32_t M_Units(void)
{
    size_t size;
    uint8_t *const data = M_ReadFont(&size);
    const uint8_t *const head = M_Table(data, "head");
    const int32_t units = head[18] << 8 | head[19];
    Mem_Free(data);
    return units;
}

TEST(default_font_renders_japanese)
{
    CHECK(IS_OK(Text_Init()));
    FONT *font = nullptr;
    CHECK(IS_OK(M_Open(16, &font)));
    TEXT_STYLE style = TextStyle_Default();
    style.color = (GFX_COLOR) { 31, 127, 211, 192 };
    int32_t width;
    int32_t height;
    CHECK(IS_OK(Text_Measure(font, "東方紅魔郷", &style, &width, &height)));
    CHECK(width > 0 && height > 0);
    IMAGE image;
    CHECK(IS_OK(
        Text_Render(font, "東方紅魔郷", &style, &g_HeapAllocator, &image)));
    CHECK_EQ_INT(image.width, width);
    CHECK_EQ_INT(image.height, height);
    size_t visible = 0;
    for (int32_t i = 0; i < image.width * image.height; i++) {
        const uint8_t *const pixel = image.pixels + (size_t)i * 4;
        if (pixel[3] != 0) {
            visible++;
            CHECK_EQ_INT(pixel[0], 31);
            CHECK_EQ_INT(pixel[1], 127);
            CHECK_EQ_INT(pixel[2], 211);
            CHECK(pixel[3] <= 192);
        }
    }
    CHECK(visible > 100);
    Image_Free(&image);
    Font_Free(font);
    M_Shutdown();
}

TEST(shadow_offsets_and_width_scale)
{
    CHECK(IS_OK(Text_Init()));
    FONT *font = nullptr;
    CHECK(IS_OK(M_Open(16, &font)));
    TEXT_STYLE style = TextStyle_Default();
    IMAGE original;
    CHECK(
        IS_OK(Text_Render(font, "Reimu", &style, &g_HeapAllocator, &original)));
    style.width_scale = 2;
    style.shadow = (GFX_COLOR) { 255, 0, 0, 255 };
    style.shadow_x = -4;
    style.shadow_y = 3;
    int32_t width;
    int32_t height;
    CHECK(IS_OK(Text_Measure(font, "Reimu", &style, &width, &height)));
    IMAGE styled;
    CHECK(IS_OK(Text_Render(font, "Reimu", &style, &g_HeapAllocator, &styled)));
    CHECK_EQ_INT(styled.width, original.width * 2 + 4);
    CHECK_EQ_INT(styled.height, original.height + 3);
    CHECK_EQ_INT(width, styled.width);
    CHECK_EQ_INT(height, styled.height);
    bool red = false;
    bool white = false;
    for (int32_t i = 0; i < styled.width * styled.height; i++) {
        const uint8_t *const pixel = styled.pixels + (size_t)i * 4;
        red |= pixel[0] != 0 && pixel[1] == 0 && pixel[3] != 0;
        white |= pixel[0] == 255 && pixel[1] == 255 && pixel[3] != 0;
    }
    CHECK(red && white);
    Image_Free(&styled);
    Image_Free(&original);
    Font_Free(font);
    M_Shutdown();
}

TEST(empty_and_invalid_text)
{
    CHECK(IS_OK(Text_Init()));
    FONT *font = nullptr;
    CHECK(IS_OK(M_Open(15, &font)));
    TEXT_STYLE style = TextStyle_Default();
    int32_t width = -1;
    int32_t height = -1;
    CHECK(IS_OK(Text_Measure(font, "", &style, &width, &height)));
    CHECK_EQ_INT(width, 0);
    CHECK_EQ_INT(height, 0);
    IMAGE image;
    CHECK(IS_OK(Text_Render(font, "", &style, &g_HeapAllocator, &image)));
    CHECK(image.pixels == nullptr);
    style.width_scale = NAN;
    CHECK(!Result_Ignore(
        Text_Render(font, "a", &style, &g_HeapAllocator, &image)));
    CHECK(image.pixels == nullptr);
    Font_Free(font);
    M_Shutdown();
}

TEST(invalid_font_and_size)
{
    FONT *font = nullptr;
    CHECK(!Result_Ignore(M_Open(15, &font)));
    CHECK(font == nullptr);
    CHECK(IS_OK(Text_Init()));
    CHECK(!Result_Ignore(M_Open(0, &font)));
    CHECK(font == nullptr);
    M_Shutdown();
}

TEST(measure_matches_glyph_bounds)
{
    CHECK(IS_OK(Text_Init()));
    FONT *font = nullptr;
    CHECK(IS_OK(M_Open(15, &font)));
    TEXT_STYLE style = TextStyle_Default();
    style.width_scale = 0.75f;
    style.shadow = (GFX_COLOR) { 0, 0, 0, 255 };
    style.shadow_x = -3;
    style.shadow_y = -2;
    const char *const strings[] = { "東方", "Á", "j", " " };
    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); i++) {
        int32_t width;
        int32_t height;
        CHECK(IS_OK(Text_Measure(font, strings[i], &style, &width, &height)));
        IMAGE image;
        CHECK(IS_OK(
            Text_Render(font, strings[i], &style, &g_HeapAllocator, &image)));
        CHECK_EQ_INT(width, image.width);
        CHECK_EQ_INT(height, image.height);
        Image_Free(&image);
    }
    Font_Free(font);
    M_Shutdown();
}

TEST(resize_uses_loaded_font_data)
{
    CHECK(IS_OK(Text_Init()));
    FONT *font = nullptr;
    CHECK(IS_OK(M_Open(15, &font)));
    const TEXT_STYLE style = TextStyle_Default();
    int32_t width;
    int32_t height;
    CHECK(IS_OK(Text_Measure(font, "東方", &style, &width, &height)));
    CHECK(IS_OK(Font_SetSize(font, 30)));
    int32_t resized_width;
    int32_t resized_height;
    CHECK(IS_OK(
        Text_Measure(font, "東方", &style, &resized_width, &resized_height)));
    CHECK(resized_width > width);
    CHECK(resized_height > height);
    CHECK(!Result_Ignore(Font_SetSize(font, NAN)));
    Font_Free(font);
    M_Shutdown();
}

// Opens the font under test with the Windows ascent and descent of its
// first face set, at pixel_height.
static FONT *M_LoadWithCell(
    const int32_t ascent, const int32_t descent, const float pixel_height)
{
    size_t size;
    uint8_t *const data = M_ReadFont(&size);
    uint8_t *const table = M_Table(data, "OS/2");
    CHECK(table != nullptr);
    table[74] = (uint8_t)(ascent >> 8);
    table[75] = (uint8_t)ascent;
    table[76] = (uint8_t)(descent >> 8);
    table[77] = (uint8_t)descent;
    FONT *const font = M_OpenData(data, size, pixel_height);
    Mem_Free(data);
    return font;
}

// Opens the font under test with a cell of its em, at pixel_height.
static FONT *M_LoadWithEmCell(const float pixel_height)
{
    const int32_t units = M_Units();
    return M_LoadWithCell(units - units / 8, units / 8, pixel_height);
}

TEST(the_font_cell_is_its_em)
{
    CHECK(IS_OK(Text_Init()));
    FONT *const bundled = M_LoadWithEmCell(30);
    FONT *const em = M_LoadWithCell(0, 0, 30);
    const TEXT_STYLE style = TextStyle_Default();
    IMAGE expected;
    IMAGE image;
    CHECK(IS_OK(Text_Render(
        em, "Night Sign 霊夢", &style, &g_HeapAllocator, &expected)));
    CHECK(IS_OK(Text_Render(
        bundled, "Night Sign 霊夢", &style, &g_HeapAllocator, &image)));
    CHECK_EQ_INT(image.width, expected.width);
    CHECK_EQ_INT(image.height, expected.height);
    CHECK(
        memcmp(
            image.pixels, expected.pixels,
            (size_t)image.width * (size_t)image.height * 4)
        == 0);
    Image_Free(&image);
    Image_Free(&expected);
    Font_Free(em);
    Font_Free(bundled);
    M_Shutdown();
}

TEST(rounded_cells_use_whole_em_pixels_and_windows_baselines)
{
    CHECK(IS_OK(Text_Init()));
    // Cells of 1.5 em give a 20-pixel em at 30 pixels.
    const int32_t units = M_Units();
    FONT *const font = M_LoadWithCell(units * 11 / 10, units * 4 / 10, 30);
    FONT *const em = M_LoadWithCell(0, 0, 20);
    FONT *const low_ascent = M_LoadWithCell(units / 2, units, 30);
    int32_t offset;
    int32_t low_offset;
    CHECK(IS_OK(Font_SetSizeRounded(font, 30, &offset)));
    CHECK(IS_OK(Font_SetSizeRounded(low_ascent, 30, &low_offset)));
    CHECK_EQ_INT(low_offset - offset, -12);
    TEXT_STYLE style = TextStyle_Default();
    style.face = TEXT_FACE_BOLD;
    IMAGE expected;
    IMAGE image;
    CHECK(IS_OK(Text_Render(
        em, "Night Sign 霊夢", &style, &g_HeapAllocator, &expected)));
    CHECK(IS_OK(Text_Render(
        font, "Night Sign 霊夢", &style, &g_HeapAllocator, &image)));
    CHECK_EQ_INT(image.width, expected.width);
    CHECK_EQ_INT(image.height, expected.height);
    CHECK(
        memcmp(
            image.pixels, expected.pixels,
            (size_t)image.width * (size_t)image.height * 4)
        == 0);
    Image_Free(&image);
    Image_Free(&expected);
    CHECK(!Result_Ignore(Font_SetSizeRounded(font, NAN, &offset)));
    CHECK(!Result_Ignore(Font_SetSizeRounded(font, 30, nullptr)));
    Font_Free(low_ascent);
    Font_Free(em);
    Font_Free(font);
    M_Shutdown();
}

TEST(gdi_raster_uses_quantized_glyph_coverage)
{
    CHECK(IS_OK(Text_Init()));
    FONT *font = nullptr;
    CHECK(IS_OK(M_Open(30, &font)));
    int32_t offset;
    CHECK(IS_OK(Font_SetSizeRounded(font, 30, &offset)));
    TEXT_STYLE style = TextStyle_Default();
    style.face = TEXT_FACE_BOLD;
    style.gdi_raster = true;
    IMAGE image;
    CHECK(IS_OK(Text_Render(font, "東", &style, &g_HeapAllocator, &image)));
    int32_t visible = 0;
    int32_t opaque = 0;
    for (int32_t i = 0; i < image.width * image.height; i++) {
        const uint8_t alpha = image.pixels[(size_t)i * 4 + 3];
        const int32_t level = (alpha * 16 + 127) / 255;
        CHECK_EQ_INT(alpha, (level * 255 + 8) / 16);
        visible += alpha != 0;
        opaque += alpha == 255;
    }
    CHECK(visible > opaque && opaque > 0);
    Image_Free(&image);
    Font_Free(font);
    M_Shutdown();
}

TEST(gdi_paint_uses_the_intensity_curve_and_five_bit_colors)
{
    IMAGE glyphs = Image_Create(5, 1, &g_HeapAllocator);
    const uint8_t coverage[5] = { 0, 16, 32, 128, 255 };
    for (int32_t i = 0; i < 5; i++) {
        glyphs.pixels[(size_t)i * 4 + 3] = coverage[i];
    }
    IMAGE surface = Image_Create(5, 1, &g_HeapAllocator);
    Text_StampGdi(&surface, &glyphs, 0, 0, (GFX_COLOR) { 240, 240, 255, 255 });
    const uint8_t expected[20] = {
        0,   0,   0,   0,   0,   0,   0,   0,   99,  99,
        107, 255, 181, 181, 189, 255, 247, 247, 255, 255,
    };
    CHECK(memcmp(surface.pixels, expected, sizeof(expected)) == 0);
    memset(surface.pixels, 255, 20);
    Text_StampGdi(&surface, &glyphs, 0, 0, (GFX_COLOR) { 0, 0, 0, 255 });
    CHECK_EQ_INT(surface.pixels[12], 189);
    CHECK_EQ_INT(surface.pixels[13], 189);
    CHECK_EQ_INT(surface.pixels[14], 189);
    Image_Free(&surface);
    Image_Free(&glyphs);
}

TEST(a_tall_cell_shrinks_the_em)
{
    CHECK(IS_OK(Text_Init()));
    const int32_t units = M_Units();
    FONT *const tall = M_LoadWithCell(units, units, 30);
    FONT *const half = M_LoadWithEmCell(15);
    const TEXT_STYLE style = TextStyle_Default();
    int32_t width;
    int32_t height;
    int32_t half_width;
    int32_t half_height;
    CHECK(IS_OK(Text_Measure(tall, "Night Sign", &style, &width, &height)));
    CHECK(IS_OK(
        Text_Measure(half, "Night Sign", &style, &half_width, &half_height)));
    CHECK_EQ_INT(width, half_width);
    CHECK(IS_OK(Font_SetSize(tall, 60)));
    CHECK(IS_OK(Font_SetSize(half, 30)));
    CHECK(IS_OK(Text_Measure(tall, "Night Sign", &style, &width, &height)));
    CHECK(IS_OK(
        Text_Measure(half, "Night Sign", &style, &half_width, &half_height)));
    CHECK_EQ_INT(width, half_width);
    Font_Free(half);
    Font_Free(tall);
    M_Shutdown();
}

// Returns the number of pixels of text that are not transparent.
static int32_t M_CountInk(
    const FONT *const font, const char *const text,
    const TEXT_STYLE *const style)
{
    IMAGE image;
    CHECK(IS_OK(Text_Render(font, text, style, &g_HeapAllocator, &image)));
    int32_t ink = 0;
    for (int32_t i = 0; i < image.width * image.height; i++) {
        ink += image.pixels[(size_t)i * 4 + 3] != 0;
    }
    Image_Free(&image);
    return ink;
}

TEST(faces_change_the_glyphs)
{
    CHECK(IS_OK(Text_Init()));
    FONT *font = nullptr;
    CHECK(IS_OK(M_Open(30, &font)));
    TEXT_STYLE style = TextStyle_Default();
    const int32_t plain = M_CountInk(font, "Reimu", &style);
    style.face = TEXT_FACE_BOLD;
    CHECK(M_CountInk(font, "Reimu", &style) > plain);
    style.face = TEXT_FACE_UNDERLINE;
    CHECK(M_CountInk(font, "Reimu", &style) > plain);
    style.face = TEXT_FACE_ITALIC;
    int32_t width;
    int32_t height;
    int32_t italic_width;
    CHECK(IS_OK(Text_Measure(font, "Reimu", &style, &italic_width, &height)));
    style.face = 0;
    CHECK(IS_OK(Text_Measure(font, "Reimu", &style, &width, &height)));
    CHECK(italic_width >= width);
    CHECK_EQ_INT(M_CountInk(font, "Reimu", &style), plain);
    Font_Free(font);
    M_Shutdown();
}

// Opens the font under test with its first face marked bold, or regular
// when bold is false.
static FONT *M_LoadWeight(const bool bold)
{
    size_t size;
    uint8_t *const data = M_ReadFont(&size);
    uint8_t *const os2 = M_Table(data, "OS/2");
    uint8_t *const head = M_Table(data, "head");
    os2[4] = bold ? 2 : 1;
    os2[5] = bold ? 0xbc : 0x90;
    os2[63] =
        bold ? (uint8_t)((os2[63] | 0x20) & ~0x40) : (uint8_t)(os2[63] & ~0x20);
    head[45] = bold ? (uint8_t)(head[45] | 1) : (uint8_t)(head[45] & ~1);
    FONT *const font = M_OpenData(data, size, 30.0f);
    Mem_Free(data);
    return font;
}

static FONT *M_LoadRegularFont(void)
{
    return M_LoadWeight(false);
}

TEST(bold_on_a_bold_font_changes_nothing)
{
    CHECK(IS_OK(Text_Init()));
    FONT *const font = M_LoadWeight(true);
    TEXT_STYLE style = TextStyle_Default();
    const int32_t plain = M_CountInk(font, "Reimu", &style);
    style.face = TEXT_FACE_BOLD;
    CHECK_EQ_INT(M_CountInk(font, "Reimu", &style), plain);
    Font_Free(font);
    M_Shutdown();
}

TEST(synthetic_bold_advances_one_pixel_per_glyph_at_every_size)
{
    CHECK(IS_OK(Text_Init()));
    FONT *const font = M_LoadRegularFont();
    const int32_t sizes[] = { 8, 15, 30, 60 };
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        CHECK(IS_OK(Font_SetSize(font, (float)sizes[i])));
        TEXT_STYLE style = TextStyle_Default();
        int32_t regular;
        int32_t bold;
        int32_t height;
        CHECK(IS_OK(Text_Measure(font, "      ", &style, &regular, &height)));
        style.face = TEXT_FACE_BOLD;
        CHECK(IS_OK(Text_Measure(font, "      ", &style, &bold, &height)));
        CHECK_EQ_INT(bold, regular + 6);
        IMAGE image;
        CHECK(IS_OK(
            Text_Render(font, "      ", &style, &g_HeapAllocator, &image)));
        CHECK_EQ_INT(image.width, bold);
        CHECK_EQ_INT(image.height, height);
        Image_Free(&image);
    }
    Font_Free(font);
    M_Shutdown();
}

TEST(synthetic_bold_moves_glyphs_without_scaling_their_pixels)
{
    CHECK(IS_OK(Text_Init()));
    FONT *const font = M_LoadRegularFont();
    TEXT_STYLE style = TextStyle_Default();
    style.face = TEXT_FACE_BOLD;
    int32_t space;
    int32_t height;
    CHECK(IS_OK(Text_Measure(font, " ", &style, &space, &height)));
    IMAGE single;
    IMAGE repeated;
    IMAGE markup;
    CHECK(IS_OK(Text_Render(font, "I", &style, &g_HeapAllocator, &single)));
    CHECK(IS_OK(Text_Render(font, "I I", &style, &g_HeapAllocator, &repeated)));
    style.face = 0;
    CHECK(IS_OK(
        Text_RenderMarkup(font, "<b$I I>", &style, &g_HeapAllocator, &markup)));
    CHECK_EQ_INT(markup.width, repeated.width);
    CHECK_EQ_INT(markup.height, repeated.height);
    CHECK(
        memcmp(
            markup.pixels, repeated.pixels,
            (size_t)markup.width * (size_t)markup.height * 4)
        == 0);
    CHECK(repeated.width >= space * 2 + single.width);
    for (int32_t y = 0; y < single.height; y++) {
        for (int32_t x = 0; x < single.width; x++) {
            const size_t from =
                ((size_t)y * (size_t)single.width + (size_t)x) * 4;
            const size_t first =
                ((size_t)y * (size_t)repeated.width + (size_t)x) * 4;
            const size_t last = first + (size_t)space * 8;
            CHECK_EQ_INT(repeated.pixels[first + 3], single.pixels[from + 3]);
            CHECK_EQ_INT(repeated.pixels[last + 3], single.pixels[from + 3]);
        }
    }
    Image_Free(&markup);
    Image_Free(&repeated);
    Image_Free(&single);
    Font_Free(font);
    M_Shutdown();
}

TEST(synthetic_bold_keeps_shaped_text_and_decorations_within_its_bounds)
{
    CHECK(IS_OK(Text_Init()));
    FONT *const font = M_LoadRegularFont();
    TEXT_STYLE style = TextStyle_Default();
    style.face = TEXT_FACE_BOLD | TEXT_FACE_ITALIC | TEXT_FACE_UNDERLINE;
    style.width_scale = 0.75f;
    style.shadow = (GFX_COLOR) { 0, 0, 0, 255 };
    style.shadow_x = -2;
    style.shadow_y = 1;
    const char *const strings[] = { "Á Â", "霊夢 A", "שלום", "مرحبا" };
    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); i++) {
        int32_t width;
        int32_t height;
        CHECK(IS_OK(Text_Measure(font, strings[i], &style, &width, &height)));
        IMAGE image;
        CHECK(IS_OK(
            Text_Render(font, strings[i], &style, &g_HeapAllocator, &image)));
        CHECK_EQ_INT(image.width, width);
        CHECK_EQ_INT(image.height, height);
        Image_Free(&image);
    }
    Font_Free(font);
    M_Shutdown();
}

static int32_t M_Width(const FONT *const font, const char *const text)
{
    const TEXT_STYLE style = TextStyle_Default();
    int32_t width;
    int32_t height;
    CHECK(IS_OK(Text_MeasureMarkup(font, text, &style, &width, &height)));
    return width;
}

// Returns the first column of image with ink, or -1.
static int32_t M_FirstInk(const IMAGE *const image)
{
    for (int32_t x = 0; x < image->width; x++) {
        for (int32_t y = 0; y < image->height; y++) {
            if (image->pixels
                    [((size_t)y * (size_t)image->width + (size_t)x) * 4 + 3]
                != 0) {
                return x;
            }
        }
    }
    return -1;
}

static FONT *M_OpenMarkupFont(void)
{
    CHECK(IS_OK(Text_Init()));
    FONT *font = nullptr;
    CHECK(IS_OK(M_Open(30, &font)));
    return font;
}

TEST(a_line_without_markup_renders_as_plain_text)
{
    FONT *const font = M_OpenMarkupFont();
    TEXT_STYLE style = TextStyle_Default();
    style.shadow = (GFX_COLOR) { 0, 0, 0, 255 };
    style.shadow_x = 3;
    style.shadow_y = 2;
    IMAGE plain;
    IMAGE markup;
    CHECK(
        IS_OK(Text_Render(font, "霊夢 <b>", &style, &g_HeapAllocator, &plain)));
    CHECK(IS_OK(Text_RenderMarkup(
        font, "霊夢 <b>", &style, &g_HeapAllocator, &markup)));
    CHECK_EQ_INT(markup.width, plain.width);
    CHECK_EQ_INT(markup.height, plain.height);
    CHECK(
        memcmp(
            markup.pixels, plain.pixels,
            (size_t)plain.width * (size_t)plain.height * 4)
        == 0);
    int32_t width;
    int32_t height;
    int32_t plain_width;
    int32_t plain_height;
    CHECK(IS_OK(Text_MeasureMarkup(font, "霊夢 <b>", &style, &width, &height)));
    CHECK(IS_OK(
        Text_Measure(font, "霊夢 <b>", &style, &plain_width, &plain_height)));
    CHECK_EQ_INT(width, plain_width);
    CHECK_EQ_INT(height, plain_height);
    Image_Free(&markup);
    Image_Free(&plain);
    Font_Free(font);
    M_Shutdown();
}

TEST(center_aligns_the_text_in_its_column)
{
    FONT *const font = M_OpenMarkupFont();
    const int32_t column = M_Width(font, "Youkai of the Dusk");
    const int32_t text = M_Width(font, "Rumia");
    CHECK_EQ_INT(M_Width(font, "<c$Rumia$Youkai of the Dusk>"), column);
    CHECK_EQ_INT(
        M_Width(font, "<c$Rumia$Youkai of the Dusk>!"),
        column + M_Width(font, "!"));
    const TEXT_STYLE style = TextStyle_Default();
    IMAGE image;
    IMAGE alone;
    CHECK(IS_OK(Text_RenderMarkup(
        font, "<c$Rumia$Youkai of the Dusk>", &style, &g_HeapAllocator,
        &image)));
    CHECK(IS_OK(Text_Render(font, "Rumia", &style, &g_HeapAllocator, &alone)));
    CHECK_EQ_INT(
        M_FirstInk(&image), column / 2 - text / 2 + M_FirstInk(&alone));
    Image_Free(&alone);
    Image_Free(&image);
    Font_Free(font);
    M_Shutdown();
}

TEST(right_aligns_the_text_at_the_column_end)
{
    FONT *const font = M_OpenMarkupFont();
    const int32_t column = M_Width(font, "12345678");
    const int32_t text = M_Width(font, "Reimu:");
    CHECK_EQ_INT(M_Width(font, "<r$Reimu:$12345678>"), column);
    const TEXT_STYLE style = TextStyle_Default();
    IMAGE image;
    IMAGE alone;
    CHECK(IS_OK(Text_RenderMarkup(
        font, "<r$Reimu:$12345678>", &style, &g_HeapAllocator, &image)));
    CHECK(IS_OK(Text_Render(font, "Reimu:", &style, &g_HeapAllocator, &alone)));
    CHECK_EQ_INT(M_FirstInk(&image), column - text + M_FirstInk(&alone));
    Image_Free(&alone);
    Image_Free(&image);
    Font_Free(font);
    M_Shutdown();
}

TEST(left_and_faces_keep_the_text_width)
{
    FONT *const font = M_OpenMarkupFont();
    const int32_t a = M_Width(font, "Reimu");
    const int32_t b = M_Width(font, " Hakurei");
    CHECK_EQ_INT(M_Width(font, "<l$Reimu> Hakurei"), a + b);
    const int32_t bold = M_Width(font, "<ub$Reimu>");
    CHECK(bold >= a + 5);
    CHECK_EQ_INT(M_Width(font, "<ub$Reimu> Hakurei"), bold + b);
    CHECK_EQ_INT(
        M_Width(font, "<l$Reimu$Reimu Hakurei>!"),
        M_Width(font, "Reimu Hakurei") + M_Width(font, "!"));
    Font_Free(font);
    M_Shutdown();
}

TEST(a_spacer_draws_nothing)
{
    FONT *const font = M_OpenMarkupFont();
    CHECK_EQ_INT(
        M_Width(font, "a<s$hidden>b"), M_Width(font, "a") + M_Width(font, "b"));
    CHECK_EQ_INT(M_Width(font, "<s$hidden$column>"), 0);
    const TEXT_STYLE style = TextStyle_Default();
    IMAGE image;
    CHECK(IS_OK(Text_RenderMarkup(
        font, "<s$hidden>", &style, &g_HeapAllocator, &image)));
    CHECK(image.pixels == nullptr);
    Font_Free(font);
    M_Shutdown();
}

TEST(a_translator_note_is_left_out)
{
    FONT *const font = M_OpenMarkupFont();
    CHECK_EQ_INT(M_Width(font, "Hello\x14note"), M_Width(font, "Hello"));
    CHECK_EQ_INT(M_Width(font, "\x14note"), 0);
    Font_Free(font);
    M_Shutdown();
}

TEST(incomplete_markup_is_text)
{
    FONT *const font = M_OpenMarkupFont();
    const TEXT_STYLE style = TextStyle_Default();
    int32_t width;
    int32_t height;
    CHECK(IS_OK(Text_Measure(font, "<c$a$b", &style, &width, &height)));
    CHECK_EQ_INT(M_Width(font, "<c$a$b"), width);
    CHECK(IS_OK(Text_Measure(font, "<text>", &style, &width, &height)));
    CHECK_EQ_INT(M_Width(font, "<text>"), width);
    CHECK_EQ_INT(
        M_Width(font, "x<c$a>"), M_Width(font, "x") + M_Width(font, "a"));
    Font_Free(font);
    M_Shutdown();
}

TEST(gdi_latin_text_keeps_individual_letters_and_advances)
{
    CHECK(IS_OK(Text_Init()));
    FONT *font = nullptr;
    CHECK(IS_OK(M_Open(30, &font)));
    TEXT_STYLE style = TextStyle_Default();
    style.face = TEXT_FACE_BOLD;
    style.gdi_raster = true;
    const char *const text = "AV Night office";
    IMAGE line;
    CHECK(IS_OK(Text_Render(font, text, &style, &g_HeapAllocator, &line)));
    int32_t pen = 0;
    for (const char *p = text; *p != 0; p++) {
        const char letter[] = { *p, 0 };
        IMAGE glyph;
        CHECK(
            IS_OK(Text_Render(font, letter, &style, &g_HeapAllocator, &glyph)));
        CHECK_EQ_INT(glyph.height, line.height);
        CHECK(pen + glyph.width <= line.width);
        for (int32_t y = 0; y < glyph.height; y++) {
            for (int32_t x = 0; x < glyph.width; x++) {
                CHECK_EQ_INT(
                    line.pixels[((size_t)y * line.width + pen + x) * 4 + 3],
                    glyph.pixels[((size_t)y * glyph.width + x) * 4 + 3]);
            }
        }
        pen += glyph.width;
        Image_Free(&glyph);
    }
    CHECK_EQ_INT(pen, line.width);
    Image_Free(&line);
    Font_Free(font);
    M_Shutdown();
}

TEST(gdi_white_text_uses_eight_bit_intensity)
{
    CHECK_EQ_INT(Text_GdiIntensity(0), 0);
    CHECK_EQ_INT(Text_GdiIntensity(16), 0);
    CHECK_EQ_INT(Text_GdiIntensity(32), 104);
    CHECK_EQ_INT(Text_GdiIntensity(128), 189);
    CHECK_EQ_INT(Text_GdiIntensity(255), 255);
}

TEST(gdi_mixed_latin_and_han_keep_individual_glyphs)
{
    CHECK(IS_OK(Text_Init()));
    FONT *font = nullptr;
    CHECK(IS_OK(M_Open(32, &font)));
    TEXT_STYLE style = TextStyle_Default();
    style.face = TEXT_FACE_BOLD;
    style.gdi_raster = true;
    IMAGE line;
    CHECK(IS_OK(Text_Render(font, "A十Vā—", &style, &g_HeapAllocator, &line)));
    const char *const letters[] = { "A", "十", "V", "ā", "—" };
    int32_t pen = 0;
    for (size_t i = 0; i < sizeof(letters) / sizeof(letters[0]); i++) {
        IMAGE glyph;
        CHECK(IS_OK(
            Text_Render(font, letters[i], &style, &g_HeapAllocator, &glyph)));
        CHECK_EQ_INT(glyph.height, line.height);
        CHECK(pen + glyph.width <= line.width);
        for (int32_t y = 0; y < glyph.height; y++) {
            for (int32_t x = 0; x < glyph.width; x++) {
                CHECK_EQ_INT(
                    line.pixels[((size_t)y * line.width + pen + x) * 4 + 3],
                    glyph.pixels[((size_t)y * glyph.width + x) * 4 + 3]);
            }
        }
        pen += glyph.width;
        Image_Free(&glyph);
    }
    CHECK_EQ_INT(pen, line.width);
    Image_Free(&line);
    Font_Free(font);
    M_Shutdown();
}

TEST(gdi_combining_accents_keep_shaped_advances)
{
    CHECK(IS_OK(Text_Init()));
    FONT *font = nullptr;
    CHECK(IS_OK(M_Open(32, &font)));
    TEXT_STYLE style = TextStyle_Default();
    style.face = TEXT_FACE_BOLD;
    style.gdi_raster = true;
    int32_t gdi;
    int32_t shaped;
    int32_t height;
    const char *const texts[] = { "é", "が", "ガ" };
    for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); i++) {
        style.gdi_raster = true;
        CHECK(IS_OK(Text_Measure(font, texts[i], &style, &gdi, &height)));
        style.gdi_raster = false;
        CHECK(IS_OK(Text_Measure(font, texts[i], &style, &shaped, &height)));
        CHECK_EQ_INT(gdi, shaped);
    }
    Font_Free(font);
    M_Shutdown();
}

// Checks that the fallback of font has its em size and measures "十　あ" as
// the font under test with a cell of that em does.
static void M_CheckFallbackEm(FONT *const font)
{
    const float em = TTF_GetFontSize(font->font);
    CHECK(TTF_GetFontSize(font->fallback->font) == em);
    FONT *const expected = M_LoadWithEmCell(em);
    TEXT_STYLE style = TextStyle_Default();
    style.face = TEXT_FACE_BOLD;
    style.gdi_raster = true;
    int32_t width;
    int32_t height;
    int32_t expected_width;
    int32_t expected_height;
    CHECK(
        IS_OK(Text_Measure(font->fallback, "十　あ", &style, &width, &height)));
    CHECK(IS_OK(Text_Measure(
        expected, "十　あ", &style, &expected_width, &expected_height)));
    CHECK_EQ_INT(width, expected_width);
    CHECK_EQ_INT(height, expected_height);
    Font_Free(expected);
}

TEST(fallback_glyphs_take_the_base_em)
{
    CHECK(IS_OK(Text_Init()));
    const int32_t units = M_Units();
    FONT *const font = M_LoadWithCell(units, units / 2, 28);
    FONT *fallback = nullptr;
    CHECK(IS_OK(M_Open(28, &fallback)));
    Font_SetFallback(font, fallback);
    int32_t offset;
    CHECK(IS_OK(Font_SetSizeRounded(font, 28, &offset)));
    CHECK(TTF_GetFontSize(font->font) != 28.0f);
    M_CheckFallbackEm(font);
    CHECK(IS_OK(Font_SetSize(font, 35)));
    M_CheckFallbackEm(font);
    Font_Free(font);
    M_Shutdown();
}

// Holds the GDI advances of bold "あ", "A" and "　" at 22 pixels in a face.
typedef struct {
    const char *family;
    int32_t kana;
    int32_t latin;
    int32_t space;
} M_ADVANCES;

#ifdef TEST_MS_GOTHIC
// Retail's advances, from GDI.
static const M_ADVANCES m_Advances[] = {
    { "MS Gothic", 23, 12, 23 },
    { "ＭＳ Ｐゴシック", 22, 15, 16 },
    { "MS UI Gothic", 19, 15, 16 },
};
#else
static const M_ADVANCES m_Advances[] = {
    { "IPAGothic", 21, 11, 21 },
    { "IPAPGothic", 19, 14, 21 },
};
#endif

TEST(each_face_has_its_gdi_advances)
{
    CHECK(IS_OK(Text_Init()));
    FontRegistry_Reset(false);
    size_t size;
    uint8_t *const data = M_ReadFont(&size);
    CHECK(IS_OK(FontRegistry_Add("font under test", data, size)));
    Mem_Free(data);
#ifndef TEST_MS_GOTHIC
    TestFonts_Add(TEST_FONT_UI_GOTHIC);
#endif
    for (size_t i = 0; i < sizeof(m_Advances) / sizeof(m_Advances[0]); i++) {
        FONT *font = nullptr;
        CHECK(IS_OK(FontRegistry_Open(m_Advances[i].family, 22, &font)));
        TEXT_STYLE style = TextStyle_Default();
        style.face = TEXT_FACE_BOLD;
        style.gdi_raster = true;
        int32_t width;
        int32_t height;
        CHECK(IS_OK(Text_Measure(font, "あ", &style, &width, &height)));
        CHECK_EQ_INT(width, m_Advances[i].kana);
        CHECK(IS_OK(Text_Measure(font, "A", &style, &width, &height)));
        CHECK_EQ_INT(width, m_Advances[i].latin);
        CHECK(IS_OK(Text_Measure(font, "　", &style, &width, &height)));
        CHECK_EQ_INT(width, m_Advances[i].space);
        Font_Free(font);
    }
    M_Shutdown();
}

// A line of 16 kanji, as th06 draws its comments in two halves, takes MS
// Gothic's width at the cell height 30, 31 pixels a bold kanji or 248 at the
// row's half size: in MS Gothic, and in IPAGothic sized em to cell.
TEST(a_kanji_line_takes_ms_gothics_width_in_the_text_font)
{
    CHECK(IS_OK(Text_Init()));
#ifdef TEST_MS_GOTHIC
    size_t size;
    uint8_t *const data = M_ReadFont(&size);
    FontRegistry_Reset(false);
    CHECK(IS_OK(FontRegistry_Add("font under test", data, size)));
    Mem_Free(data);
#else
    TestFonts_UseBuiltin();
#endif
    FONT *font = nullptr;
#ifdef TEST_MS_GOTHIC
    CHECK(IS_OK(FontRegistry_Open("ＭＳ ゴシック", 30, &font)));
#else
    CHECK(IS_OK(FontRegistry_Open("IPAGothic", 30, &font)));
    CHECK(IS_OK(Font_SetEmToCell(font)));
#endif
    int32_t offset_y;
    CHECK(IS_OK(Font_SetSizeRounded(font, 30, &offset_y)));
    TEXT_STYLE style = TextStyle_Default();
    style.face = TEXT_FACE_BOLD;
    style.gdi_raster = true;
    int32_t width;
    int32_t height;
    CHECK(IS_OK(Text_Measure(
        font, "東方紅魔郷東方紅魔郷東方紅魔郷東", &style, &width, &height)));
    CHECK_EQ_INT(width, 2 * 248);
    Font_Free(font);
    M_Shutdown();
}

#ifdef TEST_MS_GOTHIC
// Returns whether family draws "Reimu 霊夢" as other does.
static bool M_DrawsAs(const char *const family, const char *const other)
{
    FONT *font = nullptr;
    FONT *other_font = nullptr;
    CHECK(IS_OK(FontRegistry_Open(family, 30, &font)));
    CHECK(IS_OK(FontRegistry_Open(other, 30, &other_font)));
    const TEXT_STYLE style = TextStyle_Default();
    IMAGE image = {};
    IMAGE other_image = {};
    CHECK(IS_OK(
        Text_Render(font, "Reimu 霊夢", &style, &g_HeapAllocator, &image)));
    CHECK(IS_OK(Text_Render(
        other_font, "Reimu 霊夢", &style, &g_HeapAllocator, &other_image)));
    const bool same = image.width == other_image.width
        && image.height == other_image.height
        && memcmp(
               image.pixels, other_image.pixels,
               (size_t)image.width * (size_t)image.height * 4)
            == 0;
    Image_Free(&other_image);
    Image_Free(&image);
    Font_Free(other_font);
    Font_Free(font);
    return same;
}

TEST(a_patch_font_changes_the_face_that_draws)
{
    CHECK(IS_OK(Text_Init()));
    TestFonts_UseBuiltin();
    FONT *font = nullptr;
    CHECK(!Result_Ignore(FontRegistry_Open("ＭＳ ゴシック", 30, &font)));
    char *const dir = TestDir_Prepare("patch_font");
    PatchBuilder_AddPatch(dir, "a/fonts", "{fonts: {'msgothic.ttc': true}}");
    size_t size;
    uint8_t *const data = M_ReadFont(&size);
    char *const path = Str_Format("%s/a/fonts/msgothic.ttc", dir);
    TestDir_Write(path, data, size);
    const PATCH_GAME game = { .build = "v1.02h", .layout = PATCH_LAYOUT_FLAT };
    const PATCH_CHOICE choice = { .enabled = true };
    PATCH_STACK *stack = nullptr;
    CHECK(IS_OK(Patch_OpenStack(dir, "th06", &game, &choice, &stack)));
    CHECK(!M_DrawsAs("ＭＳ ゴシック", "IPAGothic"));
    CHECK(M_DrawsAs("ＭＳ ゴシック", "MS Gothic"));
    CHECK(!M_DrawsAs("MS UI Gothic", "MS Gothic"));
    Patch_CloseStack(stack);
    Mem_Free(path);
    Mem_Free(data);
    Mem_Free(dir);
    M_Shutdown();
}
#endif
