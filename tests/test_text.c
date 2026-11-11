#include "core/text.h"

#include "test.h"
#include "test_dir.h"

#include <SDL3/SDL.h>
#include <math.h>

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
    Reader_OpenBuffer(&reader, invalid, sizeof(invalid));
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

TEST(a_family_opens_the_file_that_has_it)
{
    CHECK(IS_OK(Text_Init()));
    char *const dir = TestDir_Prepare("family");
    char *const broken = TestDir_Join(dir, "broken.ttf");
    CHECK(SDL_SaveFile(broken, "not a font", 10));
    char *const missing = TestDir_Join(dir, "missing.ttf");
    const char *const paths[] = { missing, broken, SDL_getenv("TEST_FONT") };
    TEXT_FONT *font = nullptr;
    CHECK(IS_OK(TextFont_LoadFamily(paths, 3, "biz udgothic", 15, &font)));
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
    const int32_t count = data[4] << 8 | data[5];
    bool found = false;
    for (int32_t i = 0; i < count; i++) {
        const uint8_t *const record = data + 12 + i * 16;
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
    Reader_OpenBuffer(&reader, data, size);
    TEXT_FONT *font = nullptr;
    CHECK(IS_OK(TextFont_Load(&reader, pixel_height, &font)));
    Reader_Close(&reader);
    SDL_free(data);
    return font;
}

TEST(the_bundled_font_cell_is_its_em)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *bundled = nullptr;
    CHECK(IS_OK(TextFont_LoadDefault(30, &bundled)));
    TEXT_FONT *const em = M_LoadWithCell(0, 0, 30);
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

TEST(a_tall_cell_shrinks_the_em)
{
    CHECK(IS_OK(Text_Init()));
    TEXT_FONT *const tall = M_LoadWithCell(1802, 4096 - 1802, 30);
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
