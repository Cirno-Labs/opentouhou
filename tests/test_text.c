#include "core/text.h"
#include "core/text/private.h"

#include "test.h"
#include "test_dir.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

TEST(default_font_renders_japanese)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *font = nullptr;
    CHECK(IS_OK(TextFont_LoadDefault(16, &font)));
    TEXT_STYLE style = TextStyle_Default();
    style.color = (GFX_COLOR) { 31, 127, 211, 192 };
    int32_t width, height;
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
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(shadow_offsets_and_width_scale)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *font = nullptr;
    CHECK(IS_OK(TextFont_LoadDefault(16, &font)));
    TEXT_STYLE style = TextStyle_Default();
    IMAGE original;
    CHECK(
        IS_OK(Text_Render(font, "Reimu", &style, &g_HeapAllocator, &original)));
    style.width_scale = 2;
    style.shadow = (GFX_COLOR) { 255, 0, 0, 255 };
    style.shadow_x = -4;
    style.shadow_y = 3;
    int32_t width, height;
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
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(empty_and_invalid_text)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *font = nullptr;
    CHECK(IS_OK(TextFont_LoadDefault(15, &font)));
    TEXT_STYLE style = TextStyle_Default();
    int32_t width = -1, height = -1;
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
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(invalid_font_and_size)
{
    TEXT_FONT *font = nullptr;
    CHECK(!Result_Ignore(TextFont_LoadDefault(15, &font)));
    CHECK(font == nullptr);
    CHECK(IS_OK(Text_Init()));
    CHECK(!Result_Ignore(TextFont_LoadDefault(0, &font)));
    CHECK(font == nullptr);
    READER reader;
    const uint8_t invalid[] = { 1, 2, 3, 4 };
    Reader_OpenBuffer(&reader, invalid, sizeof(invalid), nullptr);
    CHECK(!Result_Ignore(TextFont_Load(&reader, 15, &font)));
    CHECK(font == nullptr);
    Reader_Close(&reader);
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(measure_matches_glyph_bounds)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *font = nullptr;
    CHECK(IS_OK(TextFont_LoadDefault(15, &font)));
    TEXT_STYLE style = TextStyle_Default();
    style.width_scale = 0.75f;
    style.shadow = (GFX_COLOR) { 0, 0, 0, 255 };
    style.shadow_x = -3;
    style.shadow_y = -2;
    const char *const strings[] = { "東方", "Á", "j", " " };
    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); i++) {
        int32_t width, height;
        CHECK(IS_OK(Text_Measure(font, strings[i], &style, &width, &height)));
        IMAGE image;
        CHECK(IS_OK(
            Text_Render(font, strings[i], &style, &g_HeapAllocator, &image)));
        CHECK_EQ_INT(width, image.width);
        CHECK_EQ_INT(height, image.height);
        Image_Free(&image);
    }
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(resize_uses_loaded_font_data)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *font = nullptr;
    CHECK(IS_OK(TextFont_LoadDefault(15, &font)));
    const TEXT_STYLE style = TextStyle_Default();
    int32_t width, height;
    CHECK(IS_OK(Text_Measure(font, "東方", &style, &width, &height)));
    CHECK(IS_OK(TextFont_SetSize(font, 30)));
    int32_t resized_width, resized_height;
    CHECK(IS_OK(
        Text_Measure(font, "東方", &style, &resized_width, &resized_height)));
    CHECK(resized_width > width);
    CHECK(resized_height > height);
    CHECK(!Result_Ignore(TextFont_SetSize(font, NAN)));
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(collection_families_use_the_same_face_for_layout_and_rasterization)
{
    CHECK(IS_OK(Text_Init()));
    const char *const paths[] = { SDL_getenv("TEST_FONT") };
    const char *const families[] = { "MS Gothic", "MS PGothic",
                                     "MS UI Gothic" };
    const int32_t advances[] = { 23, 22, 19 };
    const int32_t latin_advances[] = { 12, 15, 15 };
    const int32_t space_advances[] = { 23, 16, 16 };
    for (int32_t i = 0; i < 3; i++) {
        TEXT_FONT *font = nullptr;
        CHECK(IS_OK(TextFont_LoadFamily(paths, 1, families[i], 22, &font)));
        CHECK_EQ_STR(TTF_GetFontFamilyName(font->font), families[i]);
        CHECK(font->fallback != nullptr);
        CHECK_EQ_STR(
            TTF_GetFontFamilyName(font->fallback->font), "MS UI Gothic");
        TEXT_STYLE style = TextStyle_Default();
        style.face = TEXT_FACE_BOLD;
        int32_t width, height;
        CHECK(IS_OK(Text_Measure(font, "あ", &style, &width, &height)));
        CHECK(width > 0);
        style.gdi_raster = true;
        CHECK(IS_OK(Text_Measure(font, "あ", &style, &width, &height)));
        CHECK_EQ_INT(width, advances[i]);
        CHECK(IS_OK(Text_Measure(font, "A", &style, &width, &height)));
        CHECK_EQ_INT(width, latin_advances[i]);
        CHECK(IS_OK(Text_Measure(font, "　", &style, &width, &height)));
        CHECK_EQ_INT(width, space_advances[i]);
        IMAGE glyph;
        CHECK(IS_OK(Text_Render(font, "あ", &style, &g_HeapAllocator, &glyph)));
        CHECK_EQ_INT(glyph.width, advances[i]);
        bool antialiased = false;
        for (int32_t p = 0; p < glyph.width * glyph.height; p++) {
            const uint8_t alpha = glyph.pixels[(size_t)p * 4 + 3];
            antialiased |= alpha != 0 && alpha != 255;
        }
        CHECK(antialiased);
        Image_Free(&glyph);
        CHECK(IS_OK(Text_Render(font, "　", &style, &g_HeapAllocator, &glyph)));
        CHECK_EQ_INT(glyph.width, space_advances[i]);
        for (int32_t p = 0; p < glyph.width * glyph.height; p++) {
            CHECK_EQ_INT(glyph.pixels[(size_t)p * 4 + 3], 0);
        }
        Image_Free(&glyph);
        CHECK(IS_OK(TextFont_SetSize(font, 30)));
        CHECK_EQ_STR(TTF_GetFontFamilyName(font->font), families[i]);
        CHECK(IS_OK(Text_Measure(font, "あ", &style, &width, &height)));
        CHECK(width > advances[i]);
        TextFont_Free(font);
    }
    Text_Shutdown();
}

TEST(a_family_opens_the_file_that_has_it)
{
    CHECK(IS_OK(Text_Init()));
    char *const dir = TestDir_Prepare("family");
    char *const broken = TestDir_Join(dir, "broken.ttf");
    CHECK(SDL_SaveFile(broken, "not a font", 10));
    char *const missing = TestDir_Join(dir, "missing.ttf");
    const char *const paths[] = { missing, broken, SDL_getenv("TEST_FONT") };
    TEXT_FONT *font = nullptr;
    CHECK(IS_OK(TextFont_LoadFamily(paths, 3, "ms gothic", 15, &font)));
    CHECK(font != nullptr);
    CHECK(IS_OK(TextFont_SetSize(font, 30)));
    TEXT_FONT *bundled = nullptr;
    CHECK(IS_OK(TextFont_LoadDefault(30, &bundled)));
    const TEXT_STYLE style = TextStyle_Default();
    int32_t width, height, bundled_width, bundled_height;
    CHECK(IS_OK(Text_Measure(font, "Reimu 霊夢", &style, &width, &height)));
    CHECK(IS_OK(Text_Measure(
        bundled, "Reimu 霊夢", &style, &bundled_width, &bundled_height)));
    CHECK_EQ_INT(width, bundled_width);
    CHECK_EQ_INT(height, bundled_height);
    TextFont_Free(bundled);
    TextFont_Free(font);
    Mem_Free(missing);
    Mem_Free(broken);
    Mem_Free(dir);
    Text_Shutdown();
}

// Loads the bundled font file with its Windows ascent and descent set, and
// opens it at pixel_height.
static TEXT_FONT *M_LoadWithCell(
    const uint16_t ascent, const uint16_t descent, const float pixel_height)
{
    size_t size;
    uint8_t *const data = SDL_LoadFile(SDL_getenv("TEST_FONT"), &size);
    CHECK(data != nullptr);
    const size_t base = memcmp(data, "ttcf", 4) == 0 ? (size_t)data[12] << 24
            | (size_t)data[13] << 16 | (size_t)data[14] << 8 | data[15]
                                                     : 0;
    const int32_t count = data[base + 4] << 8 | data[base + 5];
    bool found = false;
    for (int32_t i = 0; i < count; i++) {
        const uint8_t *const record = data + base + 12 + i * 16;
        if (memcmp(record, "OS/2", 4) == 0) {
            uint8_t *const table = data
                + ((size_t)record[8] << 24 | (size_t)record[9] << 16
                   | (size_t)record[10] << 8 | record[11]);
            table[74] = (uint8_t)(ascent >> 8);
            table[75] = (uint8_t)ascent;
            table[76] = (uint8_t)(descent >> 8);
            table[77] = (uint8_t)descent;
            found = true;
        }
    }
    CHECK(found);
    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    TEXT_FONT *font = nullptr;
    CHECK(IS_OK(TextFont_Load(&reader, pixel_height, &font)));
    Reader_Close(&reader);
    SDL_free(data);
    return font;
}

TEST(the_bundled_font_uses_its_windows_cell)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *bundled = nullptr;
    CHECK(IS_OK(TextFont_LoadDefault(30, &bundled)));
    TEXT_FONT *const em = M_LoadWithCell(0, 0, 30.0f);
    const TEXT_STYLE style = TextStyle_Default();
    IMAGE expected, image;
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
    TextFont_Free(em);
    TextFont_Free(bundled);
    Text_Shutdown();
}

TEST(rounded_cells_use_whole_em_pixels_and_windows_baselines)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *const font = M_LoadWithCell(220, 80, 30);
    TEXT_FONT *const em = M_LoadWithCell(0, 0, 26);
    TEXT_FONT *const low_ascent = M_LoadWithCell(100, 200, 30);
    int32_t offset, low_offset;
    CHECK(IS_OK(TextFont_SetSizeRounded(font, 30, &offset)));
    CHECK(IS_OK(TextFont_SetSizeRounded(low_ascent, 30, &low_offset)));
    CHECK_EQ_INT(low_offset - offset, -12);
    TEXT_STYLE style = TextStyle_Default();
    style.face = TEXT_FACE_BOLD;
    IMAGE expected, image;
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
    CHECK(!Result_Ignore(TextFont_SetSizeRounded(font, NAN, &offset)));
    CHECK(!Result_Ignore(TextFont_SetSizeRounded(font, 30, nullptr)));
    TextFont_Free(low_ascent);
    TextFont_Free(em);
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(gdi_raster_uses_quantized_glyph_coverage)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *font = nullptr;
    CHECK(IS_OK(TextFont_LoadDefault(30, &font)));
    int32_t offset;
    CHECK(IS_OK(TextFont_SetSizeRounded(font, 30, &offset)));
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
    TextFont_Free(font);
    Text_Shutdown();
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
    TEXT_FONT *const tall = M_LoadWithCell(220, 512 - 220, 30);
    TEXT_FONT *half = nullptr;
    CHECK(IS_OK(TextFont_LoadDefault(15, &half)));
    const TEXT_STYLE style = TextStyle_Default();
    int32_t width, height, half_width, half_height;
    CHECK(IS_OK(Text_Measure(tall, "Night Sign", &style, &width, &height)));
    CHECK(IS_OK(
        Text_Measure(half, "Night Sign", &style, &half_width, &half_height)));
    CHECK_EQ_INT(width, half_width);
    CHECK(IS_OK(TextFont_SetSize(tall, 60)));
    CHECK(IS_OK(TextFont_SetSize(half, 30)));
    CHECK(IS_OK(Text_Measure(tall, "Night Sign", &style, &width, &height)));
    CHECK(IS_OK(
        Text_Measure(half, "Night Sign", &style, &half_width, &half_height)));
    CHECK_EQ_INT(width, half_width);
    TextFont_Free(half);
    TextFont_Free(tall);
    Text_Shutdown();
}

TEST(a_missing_family_fails)
{
    CHECK(IS_OK(Text_Init()));
    const char *const paths[] = { SDL_getenv("TEST_FONT") };
    TEXT_FONT *font = nullptr;
    CHECK(!Result_Ignore(TextFont_LoadFamily(paths, 1, "Aroania", 15, &font)));
    CHECK(font == nullptr);
    CHECK(
        !Result_Ignore(TextFont_LoadFamily(nullptr, 0, "Aroania", 15, &font)));
    Text_Shutdown();
}

// Returns the number of pixels of text that are not transparent.
static int32_t M_CountInk(
    const TEXT_FONT *const font, const char *const text,
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
    TEXT_FONT *font = nullptr;
    CHECK(IS_OK(TextFont_LoadDefault(30, &font)));
    TEXT_STYLE style = TextStyle_Default();
    const int32_t plain = M_CountInk(font, "Reimu", &style);
    style.face = TEXT_FACE_BOLD;
    CHECK(M_CountInk(font, "Reimu", &style) > plain);
    style.face = TEXT_FACE_UNDERLINE;
    CHECK(M_CountInk(font, "Reimu", &style) > plain);
    style.face = TEXT_FACE_ITALIC;
    int32_t width, height, italic_width;
    CHECK(IS_OK(Text_Measure(font, "Reimu", &style, &italic_width, &height)));
    style.face = 0;
    CHECK(IS_OK(Text_Measure(font, "Reimu", &style, &width, &height)));
    CHECK(italic_width >= width);
    CHECK_EQ_INT(M_CountInk(font, "Reimu", &style), plain);
    TextFont_Free(font);
    Text_Shutdown();
}

static TEXT_FONT *M_LoadRegularFont(void)
{
    size_t size;
    uint8_t *const data = SDL_LoadFile(SDL_getenv("TEST_FONT"), &size);
    CHECK(data != nullptr);
    const size_t base = memcmp(data, "ttcf", 4) == 0 ? (size_t)data[12] << 24
            | (size_t)data[13] << 16 | (size_t)data[14] << 8 | data[15]
                                                     : 0;
    const int32_t count = data[base + 4] << 8 | data[base + 5];
    for (int32_t i = 0; i < count; i++) {
        const uint8_t *const record = data + base + 12 + i * 16;
        uint8_t *const table = data
            + ((size_t)record[8] << 24 | (size_t)record[9] << 16
               | (size_t)record[10] << 8 | record[11]);
        if (memcmp(record, "OS/2", 4) == 0) {
            table[4] = 1;
            table[5] = 0x90;
            table[63] &= (uint8_t)~0x20;
        } else if (memcmp(record, "head", 4) == 0) {
            table[45] &= (uint8_t)~1;
        }
    }
    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    TEXT_FONT *font = nullptr;
    CHECK(IS_OK(TextFont_Load(&reader, 30.0f, &font)));
    Reader_Close(&reader);
    SDL_free(data);
    return font;
}

TEST(synthetic_bold_advances_one_pixel_per_glyph_at_every_size)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *const font = M_LoadRegularFont();
    const int32_t sizes[] = { 8, 15, 30, 60 };
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        CHECK(IS_OK(TextFont_SetSize(font, (float)sizes[i])));
        TEXT_STYLE style = TextStyle_Default();
        int32_t regular, bold, height;
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
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(synthetic_bold_moves_glyphs_without_scaling_their_pixels)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *const font = M_LoadRegularFont();
    TEXT_STYLE style = TextStyle_Default();
    style.face = TEXT_FACE_BOLD;
    int32_t space, height;
    CHECK(IS_OK(Text_Measure(font, " ", &style, &space, &height)));
    IMAGE single, repeated, markup;
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
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(synthetic_bold_keeps_shaped_text_and_decorations_within_its_bounds)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *const font = M_LoadRegularFont();
    TEXT_STYLE style = TextStyle_Default();
    style.face = TEXT_FACE_BOLD | TEXT_FACE_ITALIC | TEXT_FACE_UNDERLINE;
    style.width_scale = 0.75f;
    style.shadow = (GFX_COLOR) { 0, 0, 0, 255 };
    style.shadow_x = -2;
    style.shadow_y = 1;
    const char *const strings[] = { "Á Â", "霊夢 A", "שלום", "مرحبا" };
    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); i++) {
        int32_t width, height;
        CHECK(IS_OK(Text_Measure(font, strings[i], &style, &width, &height)));
        IMAGE image;
        CHECK(IS_OK(
            Text_Render(font, strings[i], &style, &g_HeapAllocator, &image)));
        CHECK_EQ_INT(image.width, width);
        CHECK_EQ_INT(image.height, height);
        Image_Free(&image);
    }
    TextFont_Free(font);
    Text_Shutdown();
}

static int32_t M_Width(const TEXT_FONT *const font, const char *const text)
{
    const TEXT_STYLE style = TextStyle_Default();
    int32_t width, height;
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

static TEXT_FONT *M_OpenMarkupFont(void)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *font = nullptr;
    CHECK(IS_OK(TextFont_LoadDefault(30, &font)));
    return font;
}

TEST(a_line_without_markup_renders_as_plain_text)
{
    TEXT_FONT *const font = M_OpenMarkupFont();
    TEXT_STYLE style = TextStyle_Default();
    style.shadow = (GFX_COLOR) { 0, 0, 0, 255 };
    style.shadow_x = 3;
    style.shadow_y = 2;
    IMAGE plain, markup;
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
    int32_t width, height, plain_width, plain_height;
    CHECK(IS_OK(Text_MeasureMarkup(font, "霊夢 <b>", &style, &width, &height)));
    CHECK(IS_OK(
        Text_Measure(font, "霊夢 <b>", &style, &plain_width, &plain_height)));
    CHECK_EQ_INT(width, plain_width);
    CHECK_EQ_INT(height, plain_height);
    Image_Free(&markup);
    Image_Free(&plain);
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(center_aligns_the_text_in_its_column)
{
    TEXT_FONT *const font = M_OpenMarkupFont();
    const int32_t column = M_Width(font, "Youkai of the Dusk");
    const int32_t text = M_Width(font, "Rumia");
    CHECK_EQ_INT(M_Width(font, "<c$Rumia$Youkai of the Dusk>"), column);
    CHECK_EQ_INT(
        M_Width(font, "<c$Rumia$Youkai of the Dusk>!"),
        column + M_Width(font, "!"));
    const TEXT_STYLE style = TextStyle_Default();
    IMAGE image, alone;
    CHECK(IS_OK(Text_RenderMarkup(
        font, "<c$Rumia$Youkai of the Dusk>", &style, &g_HeapAllocator,
        &image)));
    CHECK(IS_OK(Text_Render(font, "Rumia", &style, &g_HeapAllocator, &alone)));
    CHECK_EQ_INT(
        M_FirstInk(&image), column / 2 - text / 2 + M_FirstInk(&alone));
    Image_Free(&alone);
    Image_Free(&image);
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(right_aligns_the_text_at_the_column_end)
{
    TEXT_FONT *const font = M_OpenMarkupFont();
    const int32_t column = M_Width(font, "12345678");
    const int32_t text = M_Width(font, "Reimu:");
    CHECK_EQ_INT(M_Width(font, "<r$Reimu:$12345678>"), column);
    const TEXT_STYLE style = TextStyle_Default();
    IMAGE image, alone;
    CHECK(IS_OK(Text_RenderMarkup(
        font, "<r$Reimu:$12345678>", &style, &g_HeapAllocator, &image)));
    CHECK(IS_OK(Text_Render(font, "Reimu:", &style, &g_HeapAllocator, &alone)));
    CHECK_EQ_INT(M_FirstInk(&image), column - text + M_FirstInk(&alone));
    Image_Free(&alone);
    Image_Free(&image);
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(left_markup_preserves_width_and_bold_advances)
{
    TEXT_FONT *const font = M_OpenMarkupFont();
    const int32_t a = M_Width(font, "Reimu");
    const int32_t b = M_Width(font, " Hakurei");
    CHECK_EQ_INT(M_Width(font, "<l$Reimu> Hakurei"), a + b);
    const int32_t bold = M_Width(font, "<ub$Reimu>");
    CHECK(bold >= a + 5);
    CHECK_EQ_INT(M_Width(font, "<ub$Reimu> Hakurei"), bold + b);
    CHECK_EQ_INT(
        M_Width(font, "<l$Reimu$Reimu Hakurei>!"),
        M_Width(font, "Reimu Hakurei") + M_Width(font, "!"));
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(a_spacer_draws_nothing)
{
    TEXT_FONT *const font = M_OpenMarkupFont();
    CHECK_EQ_INT(
        M_Width(font, "a<s$hidden>b"), M_Width(font, "a") + M_Width(font, "b"));
    CHECK_EQ_INT(M_Width(font, "<s$hidden$column>"), 0);
    const TEXT_STYLE style = TextStyle_Default();
    IMAGE image;
    CHECK(IS_OK(Text_RenderMarkup(
        font, "<s$hidden>", &style, &g_HeapAllocator, &image)));
    CHECK(image.pixels == nullptr);
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(a_translator_note_is_left_out)
{
    TEXT_FONT *const font = M_OpenMarkupFont();
    CHECK_EQ_INT(M_Width(font, "Hello\x14note"), M_Width(font, "Hello"));
    CHECK_EQ_INT(M_Width(font, "\x14note"), 0);
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(incomplete_markup_is_text)
{
    TEXT_FONT *const font = M_OpenMarkupFont();
    const TEXT_STYLE style = TextStyle_Default();
    int32_t width, height;
    CHECK(IS_OK(Text_Measure(font, "<c$a$b", &style, &width, &height)));
    CHECK_EQ_INT(M_Width(font, "<c$a$b"), width);
    CHECK(IS_OK(Text_Measure(font, "<text>", &style, &width, &height)));
    CHECK_EQ_INT(M_Width(font, "<text>"), width);
    CHECK_EQ_INT(
        M_Width(font, "x<c$a>"), M_Width(font, "x") + M_Width(font, "a"));
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(gdi_latin_text_keeps_individual_letters_and_advances)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *font = nullptr;
    CHECK(IS_OK(TextFont_LoadDefault(30, &font)));
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
    TextFont_Free(font);
    Text_Shutdown();
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
    TEXT_FONT *font = nullptr;
    CHECK(IS_OK(TextFont_LoadDefault(32, &font)));
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
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(gdi_combining_accents_keep_shaped_advances)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *font = nullptr;
    CHECK(IS_OK(TextFont_LoadDefault(32, &font)));
    TEXT_STYLE style = TextStyle_Default();
    style.face = TEXT_FACE_BOLD;
    style.gdi_raster = true;
    int32_t gdi, shaped, height;
    const char *const texts[] = { "é", "が", "ガ" };
    for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); i++) {
        style.gdi_raster = true;
        CHECK(IS_OK(Text_Measure(font, texts[i], &style, &gdi, &height)));
        style.gdi_raster = false;
        CHECK(IS_OK(Text_Measure(font, texts[i], &style, &shaped, &height)));
        CHECK_EQ_INT(gdi, shaped);
    }
    TextFont_Free(font);
    Text_Shutdown();
}

TEST(fallback_glyphs_follow_the_base_character_size)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *const font = M_LoadWithCell(220, 80, 28);
    CHECK(IS_OK(TextFont_LoadDefault(28, &font->fallback)));
    TEXT_FONT *expected = nullptr;
    CHECK(IS_OK(TextFont_LoadDefault(24, &expected)));
    int32_t offset;
    CHECK(IS_OK(TextFont_SetSizeRounded(font, 28, &offset)));
    TEXT_STYLE style = TextStyle_Default();
    style.face = TEXT_FACE_BOLD;
    style.gdi_raster = true;
    int32_t width, height, expected_width, expected_height;
    CHECK(
        IS_OK(Text_Measure(font->fallback, "十　あ", &style, &width, &height)));
    CHECK(IS_OK(Text_Measure(
        expected, "十　あ", &style, &expected_width, &expected_height)));
    CHECK_EQ_INT(width, expected_width);
    CHECK_EQ_INT(height, expected_height);
    CHECK(IS_OK(TextFont_SetSize(font, 35)));
    CHECK(IS_OK(TextFont_SetSize(expected, 35.0f * 256.0f / 300.0f)));
    CHECK(
        IS_OK(Text_Measure(font->fallback, "十　あ", &style, &width, &height)));
    CHECK(IS_OK(Text_Measure(
        expected, "十　あ", &style, &expected_width, &expected_height)));
    CHECK_EQ_INT(width, expected_width);
    CHECK_EQ_INT(height, expected_height);
    TextFont_Free(expected);
    TextFont_Free(font);
    Text_Shutdown();
}
