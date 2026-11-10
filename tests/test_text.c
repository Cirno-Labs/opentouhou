#include "core/text.h"

#include "test.h"

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
