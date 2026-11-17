#include "th07/anm.h"
#include "th07/anm/private.h"

#include "test.h"

#include "core/base/memory.h"

#include <stdint.h>

static const uint8_t *M_Pixel(
    const IMAGE *const image, const int32_t x, const int32_t y)
{
    return image->pixels + ((size_t)y * (size_t)image->width + (size_t)x) * 4;
}

// Returns the first column with an opaque pixel in rows row to row + 14.
static int32_t M_FirstColumn(const IMAGE *const image, const int32_t row)
{
    for (int32_t x = 0; x < image->width; x++) {
        for (int32_t y = row; y < row + 15; y++) {
            if (M_Pixel(image, x, y)[3] > 32) {
                return x;
            }
        }
    }
    return -1;
}

static int32_t M_LastRow(const IMAGE *const image)
{
    int32_t last = -1;
    for (int32_t y = 0; y < image->height; y++) {
        for (int32_t x = 0; x < image->width; x++) {
            if (M_Pixel(image, x, y)[3] > 32) {
                last = y;
            }
        }
    }
    return last;
}

static int32_t M_MaxRed(const IMAGE *const image, const int32_t y)
{
    int32_t red = 0;
    for (int32_t x = 0; x < image->width; x++) {
        const uint8_t *const pixel = M_Pixel(image, x, y);
        if (pixel[3] > 128 && pixel[0] > red) {
            red = pixel[0];
        }
    }
    return red;
}

static ANM_SPRITE M_Sprite(const int32_t texture_width)
{
    return (ANM_SPRITE) {
        .slot = 100,
        .y0 = 16.0f,
        .x1 = 128.0f,
        .y1 = 31.0f,
        .texture_width = (float)texture_width,
        .texture_height = 64.0f,
        .width = 128.0f,
        .height = 15.0f,
        .scale_x = 1.0f,
        .scale_y = 1.0f,
    };
}

static void M_Setup(const int32_t texture_width)
{
    CHECK(IS_OK(Anm_InitText(nullptr, nullptr, 0)));
    CHECK(IS_OK(Anm_CreateTexture(100, texture_width, 64, ANM_TEXTURE_RGBA8)));
}

static void M_Teardown(void)
{
    Anm_ReleaseTexture(100);
    Anm_ShutdownText();
}

TEST(text_fills_the_sprite_rows)
{
    M_Setup(512);
    const ANM_SPRITE sprite = M_Sprite(512);
    ANM_VM vm = { .sprite_data = &sprite, .font_width = 15, .font_height = 15 };
    CHECK(IS_OK(Anm_RenderText(&vm, 0x0000ff, 0, "%s", "■")));
    CHECK(vm.shown);
    const IMAGE *const image = AnmTexture_GetImage(100);
    const int32_t left = M_FirstColumn(image, 16);
    CHECK(left >= 0 && left < 4);
    CHECK_EQ_INT(M_FirstColumn(image, 0), -1);
    const int32_t last = M_LastRow(image);
    CHECK(last >= 26 && last <= 30);
    for (int32_t i = 0; i < image->width * image->height; i++) {
        const uint8_t *const pixel = image->pixels + (size_t)i * 4;
        CHECK(pixel[3] < 128 || pixel[2] < 32);
    }
    M_Teardown();
}

TEST(original_text_aligns_by_its_cp932_byte_count)
{
    M_Setup(512);
    const ANM_SPRITE sprite = M_Sprite(512);
    ANM_VM vm = { .sprite_data = &sprite, .font_width = 15, .font_height = 15 };
    CHECK(IS_OK(Anm_RenderText(&vm, 0xffffff, 0, "%s", "■A■")));
    const int32_t left = M_FirstColumn(AnmTexture_GetImage(100), 16);
    CHECK(IS_OK(Anm_RenderTextRightAligned(&vm, 0xffffff, 0, "%s", "■A■")));
    const int32_t right = M_FirstColumn(AnmTexture_GetImage(100), 16);
    CHECK_EQ_INT(right - left, 90);
    CHECK(IS_OK(Anm_RenderTextCentered(&vm, 0xffffff, 0, "%s", "■A■")));
    const int32_t center = M_FirstColumn(AnmTexture_GetImage(100), 16);
    CHECK_EQ_INT(center - left, 45);
    M_Teardown();
}

TEST(text_darkens_downwards_less_without_shadow)
{
    M_Setup(512);
    const ANM_SPRITE sprite = M_Sprite(512);
    ANM_VM vm = { .sprite_data = &sprite, .font_width = 15, .font_height = 15 };
    CHECK(IS_OK(Anm_RenderText(&vm, 0x0000ff, 0, "%s", "■")));
    const IMAGE *const image = AnmTexture_GetImage(100);
    const int32_t top = M_MaxRed(image, 19);
    const int32_t bottom = M_MaxRed(image, 26);
    CHECK(top > bottom + 40);
    CHECK(IS_OK(Anm_RenderText(&vm, 0x0000ff, UINT32_MAX, "%s", "■")));
    const int32_t light = M_MaxRed(AnmTexture_GetImage(100), 26);
    CHECK(light > bottom + 20);
    M_Teardown();
}

// Retail shades only 2 * width * (2 * 15 + 6) pixels of its 1024-wide
// surface, so a narrow row loses the bottom of its text.
TEST(narrow_text_rows_lose_their_bottom)
{
    M_Setup(256);
    const ANM_SPRITE sprite = M_Sprite(256);
    ANM_VM vm = { .sprite_data = &sprite, .font_width = 15, .font_height = 15 };
    CHECK(IS_OK(Anm_RenderText(&vm, 0xffffff, 0, "%s", "■")));
    CHECK(M_LastRow(AnmTexture_GetImage(100)) <= 16 + 9);
    M_Teardown();
}
