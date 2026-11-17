#include "core/image.h"

#include "test.h"

static IMAGE M_Make2x2(
    const uint8_t p0[4], const uint8_t p1[4], const uint8_t p2[4],
    const uint8_t p3[4])
{
    IMAGE image = Image_Create(2, 2, &g_HeapAllocator);
    for (int i = 0; i < 4; i++) {
        image.pixels[0 * 4 + i] = p0[i];
        image.pixels[1 * 4 + i] = p1[i];
        image.pixels[2 * 4 + i] = p2[i];
        image.pixels[3 * 4 + i] = p3[i];
    }
    return image;
}

TEST(create_is_zeroed)
{
    IMAGE image = Image_Create(3, 2, &g_HeapAllocator);
    CHECK_EQ_INT(image.width, 3);
    CHECK_EQ_INT(image.height, 2);
    for (int i = 0; i < 3 * 2 * 4; i++) {
        CHECK_EQ_INT(image.pixels[i], 0);
    }
    Image_Free(&image);
}

// Fills each pixel with its position, as R = x, G = y, B = tag, A = 255.
static IMAGE M_MakeNumbered(
    const int32_t width, const int32_t height, const uint8_t tag)
{
    IMAGE image = Image_Create(width, height, &g_HeapAllocator);
    for (int32_t y = 0; y < height; y++) {
        for (int32_t x = 0; x < width; x++) {
            uint8_t *const pixel = &image.pixels[(y * width + x) * 4];
            pixel[0] = (uint8_t)x;
            pixel[1] = (uint8_t)y;
            pixel[2] = tag;
            pixel[3] = 255;
        }
    }
    return image;
}

static bool M_PixelIs(
    const IMAGE *const image, const int32_t x, const int32_t y, const uint8_t r,
    const uint8_t g, const uint8_t b)
{
    const uint8_t *const pixel = &image->pixels[(y * image->width + x) * 4];
    return pixel[0] == r && pixel[1] == g && pixel[2] == b && pixel[3] == 255;
}

static bool M_IsNumbered(const IMAGE *const image, const uint8_t tag)
{
    for (int32_t y = 0; y < image->height; y++) {
        for (int32_t x = 0; x < image->width; x++) {
            if (!M_PixelIs(image, x, y, (uint8_t)x, (uint8_t)y, tag)) {
                return false;
            }
        }
    }
    return true;
}

TEST(crop_copies_the_rectangle)
{
    IMAGE image = M_MakeNumbered(4, 3, 7);
    IMAGE out;
    CHECK(IS_OK(Image_Crop(&image, 1, 1, 2, 2, &g_HeapAllocator, &out)));
    CHECK_EQ_INT(out.width, 2);
    CHECK_EQ_INT(out.height, 2);
    CHECK(M_PixelIs(&out, 0, 0, 1, 1, 7));
    CHECK(M_PixelIs(&out, 1, 0, 2, 1, 7));
    CHECK(M_PixelIs(&out, 0, 1, 1, 2, 7));
    CHECK(M_PixelIs(&out, 1, 1, 2, 2, 7));
    Image_Free(&out);
    Image_Free(&image);
}

TEST(crop_reaches_the_edges)
{
    IMAGE image = M_MakeNumbered(4, 3, 7);
    IMAGE out;
    CHECK(IS_OK(Image_Crop(&image, 0, 0, 4, 3, &g_HeapAllocator, &out)));
    CHECK(M_IsNumbered(&out, 7));
    Image_Free(&out);
    CHECK(IS_OK(Image_Crop(&image, 3, 2, 1, 1, &g_HeapAllocator, &out)));
    CHECK(M_PixelIs(&out, 0, 0, 3, 2, 7));
    Image_Free(&out);
    Image_Free(&image);
}

TEST(crop_rejects_rectangles_outside_the_image)
{
    IMAGE image = M_MakeNumbered(4, 3, 7);
    const int32_t rects[][4] = {
        { -1, 0, 2, 2 },        { 0, -1, 2, 2 }, { 3, 0, 2, 1 },
        { 0, 2, 1, 2 },         { 0, 0, -1, 1 }, { 0, 0, 1, -1 },
        { 5, 0, 0, 0 },         { 0, 0, 5, 3 },  { INT32_MAX, 0, 1, 1 },
        { 1, 0, INT32_MAX, 1 },
    };
    for (size_t i = 0; i < sizeof(rects) / sizeof(rects[0]); i++) {
        IMAGE out = {};
        const RESULT result = Image_Crop(
            &image, rects[i][0], rects[i][1], rects[i][2], rects[i][3],
            &g_HeapAllocator, &out);
        CHECK(!IS_OK(result));
        Result_Ignore(result);
        CHECK(out.pixels == nullptr);
    }
    CHECK(M_IsNumbered(&image, 7));
    Image_Free(&image);
}

TEST(crop_of_an_empty_rectangle_is_empty)
{
    IMAGE image = M_MakeNumbered(4, 3, 7);
    IMAGE out;
    CHECK(IS_OK(Image_Crop(&image, 4, 3, 0, 0, &g_HeapAllocator, &out)));
    CHECK_EQ_INT(out.width, 0);
    CHECK_EQ_INT(out.height, 0);
    CHECK(out.pixels == nullptr);
    Image_Free(&out);
    CHECK(IS_OK(Image_Crop(&image, 1, 1, 0, 2, &g_HeapAllocator, &out)));
    CHECK_EQ_INT(out.width, 0);
    CHECK_EQ_INT(out.height, 2);
    Image_Free(&out);
    Image_Free(&image);
}

TEST(paste_copies_into_the_rectangle)
{
    IMAGE image = M_MakeNumbered(4, 3, 7);
    IMAGE source = M_MakeNumbered(2, 2, 9);
    CHECK(IS_OK(Image_Paste(&image, 1, 1, &source)));
    for (int32_t y = 0; y < 3; y++) {
        for (int32_t x = 0; x < 4; x++) {
            const bool inside = x >= 1 && x < 3 && y >= 1;
            CHECK(
                inside
                    ? M_PixelIs(
                          &image, x, y, (uint8_t)(x - 1), (uint8_t)(y - 1), 9)
                    : M_PixelIs(&image, x, y, (uint8_t)x, (uint8_t)y, 7));
        }
    }
    Image_Free(&source);
    Image_Free(&image);
}

TEST(paste_reaches_the_edges)
{
    IMAGE image = M_MakeNumbered(4, 3, 7);
    IMAGE whole = M_MakeNumbered(4, 3, 9);
    CHECK(IS_OK(Image_Paste(&image, 0, 0, &whole)));
    CHECK(M_IsNumbered(&image, 9));
    IMAGE corner = M_MakeNumbered(1, 1, 5);
    CHECK(IS_OK(Image_Paste(&image, 3, 2, &corner)));
    CHECK(M_PixelIs(&image, 3, 2, 0, 0, 5));
    CHECK(M_PixelIs(&image, 2, 2, 2, 2, 9));
    Image_Free(&corner);
    Image_Free(&whole);
    Image_Free(&image);
}

TEST(paste_rejects_sources_outside_the_image)
{
    IMAGE image = M_MakeNumbered(4, 3, 7);
    IMAGE source = M_MakeNumbered(2, 2, 9);
    const int32_t positions[][2] = {
        { -1, 0 }, { 0, -1 }, { 3, 0 }, { 0, 2 }, { INT32_MAX, 0 },
    };
    for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
        const RESULT result =
            Image_Paste(&image, positions[i][0], positions[i][1], &source);
        CHECK(!IS_OK(result));
        Result_Ignore(result);
    }
    IMAGE large = M_MakeNumbered(5, 1, 9);
    const RESULT result = Image_Paste(&image, 0, 0, &large);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    CHECK(M_IsNumbered(&image, 7));
    Image_Free(&large);
    Image_Free(&source);
    Image_Free(&image);
}

TEST(paste_of_an_empty_image_changes_nothing)
{
    IMAGE image = M_MakeNumbered(4, 3, 7);
    IMAGE empty = Image_Create(0, 0, &g_HeapAllocator);
    CHECK(IS_OK(Image_Paste(&image, 4, 3, &empty)));
    IMAGE column = Image_Create(0, 3, &g_HeapAllocator);
    CHECK(IS_OK(Image_Paste(&image, 2, 0, &column)));
    CHECK(M_IsNumbered(&image, 7));
    const RESULT result = Image_Paste(&image, 5, 0, &empty);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    Image_Free(&column);
    Image_Free(&empty);
    Image_Free(&image);
}

TEST(color_key_clears_matching_alpha)
{
    const uint8_t black[4] = { 0, 0, 0, 255 };
    const uint8_t white[4] = { 255, 255, 255, 255 };
    IMAGE image = M_Make2x2(black, white, black, white);
    Image_ColorKey(&image, 0x000000);
    CHECK_EQ_INT(image.pixels[0 * 4 + 3], 0);
    CHECK_EQ_INT(image.pixels[1 * 4 + 3], 255);
    CHECK_EQ_INT(image.pixels[2 * 4 + 3], 0);
    CHECK_EQ_INT(image.pixels[3 * 4 + 3], 255);
    Image_Free(&image);
}

TEST(alpha_from_copies_blue_channel)
{
    const uint8_t a[4] = { 10, 20, 30, 255 };
    IMAGE image = M_Make2x2(a, a, a, a);
    const uint8_t m0[4] = { 0, 0, 1, 0 };
    const uint8_t m1[4] = { 0, 0, 254, 0 };
    IMAGE mask = M_Make2x2(m0, m1, m0, m1);
    CHECK(IS_OK(Image_AlphaFrom(&image, &mask)));
    CHECK_EQ_INT(image.pixels[0 * 4 + 3], 1);
    CHECK_EQ_INT(image.pixels[1 * 4 + 3], 254);
    Image_Free(&image);
    Image_Free(&mask);
}

TEST(alpha_from_rejects_size_mismatch)
{
    const uint8_t a[4] = { 0, 0, 0, 0 };
    IMAGE image = M_Make2x2(a, a, a, a);
    IMAGE mask = Image_Create(1, 1, &g_HeapAllocator);
    RESULT result = Image_AlphaFrom(&image, &mask);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    Image_Free(&image);
    Image_Free(&mask);
}

TEST(quantize_r5g6b5_forces_opaque)
{
    const uint8_t p[4] = { 255, 128, 16, 0 };
    IMAGE image = M_Make2x2(p, p, p, p);
    Image_Quantize(&image, IMAGE_DEPTH_R5G6B5);
    CHECK_EQ_INT(image.pixels[0], 255);
    CHECK_EQ_INT(image.pixels[1], 130);
    CHECK_EQ_INT(image.pixels[2], 16);
    CHECK_EQ_INT(image.pixels[3], 255);
    Image_Free(&image);
}

TEST(quantize_a1r5g5b5_rounds_alpha_to_one_bit)
{
    const uint8_t low[4] = { 0, 0, 0, 1 };
    const uint8_t high[4] = { 0, 0, 0, 200 };
    IMAGE image = M_Make2x2(low, high, low, high);
    Image_Quantize(&image, IMAGE_DEPTH_A1R5G5B5);
    CHECK_EQ_INT(image.pixels[0 * 4 + 3], 0);
    CHECK_EQ_INT(image.pixels[1 * 4 + 3], 255);
    Image_Free(&image);
}

TEST(quantize_a4r4g4b4_replicates_nibble)
{
    const uint8_t p[4] = { 0x3f, 0x3f, 0x3f, 0x3f };
    IMAGE image = M_Make2x2(p, p, p, p);
    Image_Quantize(&image, IMAGE_DEPTH_A4R4G4B4);
    CHECK_EQ_INT(image.pixels[0], 0x44);
    Image_Free(&image);
}

TEST(quantize_rounds_channels_up_and_down)
{
    const uint8_t low[4] = { 15, 15, 15, 15 };
    const uint8_t high[4] = { 240, 240, 240, 240 };
    IMAGE image = M_Make2x2(low, high, low, high);
    Image_Quantize(&image, IMAGE_DEPTH_A4R4G4B4);
    for (int32_t channel = 0; channel < 4; channel++) {
        CHECK_EQ_INT(image.pixels[channel], 17);
        CHECK_EQ_INT(image.pixels[4 + channel], 238);
    }
    Image_Free(&image);
    const uint8_t rgb[4] = { 5, 3, 250, 0 };
    image = M_Make2x2(rgb, rgb, rgb, rgb);
    Image_Quantize(&image, IMAGE_DEPTH_R5G6B5);
    CHECK_EQ_INT(image.pixels[0], 8);
    CHECK_EQ_INT(image.pixels[1], 4);
    CHECK_EQ_INT(image.pixels[2], 247);
    Image_Free(&image);
}

TEST(resize_keeps_a_flat_color)
{
    IMAGE image = Image_Create(4, 4, &g_HeapAllocator);
    for (int i = 0; i < 4 * 4; i++) {
        image.pixels[i * 4 + 0] = 100;
        image.pixels[i * 4 + 1] = 150;
        image.pixels[i * 4 + 2] = 200;
        image.pixels[i * 4 + 3] = 255;
    }
    Image_Resize(&image, 2, 2);
    CHECK_EQ_INT(image.width, 2);
    CHECK_EQ_INT(image.height, 2);
    for (int i = 0; i < 2 * 2; i++) {
        CHECK_EQ_INT(image.pixels[i * 4 + 0], 100);
        CHECK_EQ_INT(image.pixels[i * 4 + 1], 150);
        CHECK_EQ_INT(image.pixels[i * 4 + 2], 200);
        CHECK_EQ_INT(image.pixels[i * 4 + 3], 255);
    }
    Image_Free(&image);
}

TEST(resize_interpolates_a_gradient)
{
    IMAGE image = Image_Create(2, 1, &g_HeapAllocator);
    image.pixels[0] = 0;
    image.pixels[4] = 100;
    Image_Resize(&image, 4, 1);
    CHECK_EQ_INT(image.pixels[0 * 4], 0);
    CHECK_EQ_INT(image.pixels[3 * 4], 100);
    Image_Free(&image);
}

TEST(triangle_downsampling_wraps_the_source_edges)
{
    IMAGE image = Image_Create(4, 4, &g_HeapAllocator);
    memset(image.pixels, 255, 4);
    Image_DownsampleTriangle(
        &image, 2, 2, IMAGE_DEPTH_A1R5G5B5, IMAGE_DEPTH_RGBA8);
    const uint8_t expected[] = { 49, 7, 7, 1 };
    for (int32_t i = 0; i < 4; i++) {
        for (int32_t c = 0; c < 4; c++) {
            CHECK_EQ_INT(image.pixels[i * 4 + c], expected[i]);
        }
    }
    Image_Free(&image);
}

TEST(triangle_downsampling_keeps_source_channel_precision)
{
    IMAGE image = Image_Create(4, 4, &g_HeapAllocator);
    for (int32_t i = 0; i < 16; i++) {
        image.pixels[i * 4] = 24;
        image.pixels[i * 4 + 3] = 255;
    }
    Image_DownsampleTriangle(
        &image, 2, 2, IMAGE_DEPTH_A1R5G5B5, IMAGE_DEPTH_RGBA8);
    for (int32_t i = 0; i < 4; i++) {
        CHECK_EQ_INT(image.pixels[i * 4], 25);
        CHECK_EQ_INT(image.pixels[i * 4 + 1], 0);
        CHECK_EQ_INT(image.pixels[i * 4 + 2], 0);
        CHECK_EQ_INT(image.pixels[i * 4 + 3], 255);
    }
    Image_Free(&image);
}

TEST(triangle_filter_keeps_both_axes_when_one_size_changes)
{
    IMAGE image = Image_Create(4, 4, &g_HeapAllocator);
    memset(image.pixels, 255, 4);
    Image_DownsampleTriangle(
        &image, 4, 2, IMAGE_DEPTH_A1R5G5B5, IMAGE_DEPTH_RGBA8);
    const uint8_t expected[] = { 84, 14, 0, 14, 12, 2, 0, 2 };
    for (int32_t i = 0; i < 8; i++) {
        for (int32_t c = 0; c < 4; c++) {
            CHECK_EQ_INT(image.pixels[i * 4 + c], expected[i]);
        }
    }
    Image_Free(&image);
}

TEST(dithered_surface_copies_keep_the_ordered_packed_colors)
{
    static const struct {
        IMAGE_DEPTH depth;
        uint16_t pixels[16];
    } cases[] = {
        { IMAGE_DEPTH_R5G6B5,
          {
              0x8410,
              0x8410,
              0x8410,
              0x7bef,
              0x7bef,
              0x8410,
              0x7bef,
              0x8410,
              0x8410,
              0x7bef,
              0x8410,
              0x7c0f,
              0x7bef,
              0x8410,
              0x7bef,
              0x8410,
          } },
        { IMAGE_DEPTH_A1R5G5B5,
          {
              0xc210,
              0x4210,
              0xc210,
              0x3def,
              0x3def,
              0xc210,
              0x3def,
              0xc210,
              0xc210,
              0x3def,
              0xc210,
              0x3def,
              0x3def,
              0xc210,
              0x3def,
              0xc210,
          } },
        { IMAGE_DEPTH_A4R4G4B4,
          {
              0x8888,
              0x7777,
              0x8888,
              0x7777,
              0x7777,
              0x8888,
              0x7777,
              0x8888,
              0x8888,
              0x7777,
              0x8888,
              0x7777,
              0x7777,
              0x8888,
              0x7777,
              0x8888,
          } },
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        IMAGE image = Image_Create(8, 8, &g_HeapAllocator);
        memset(image.pixels, 128, 8 * 8 * 4);
        Image_QuantizeDithered(&image, cases[c].depth);
        for (int32_t y = 0; y < 8; y++) {
            for (int32_t x = 0; x < 8; x++) {
                const uint8_t *const pixel = image.pixels + (y * 8 + x) * 4;
                uint16_t packed;
                if (cases[c].depth == IMAGE_DEPTH_R5G6B5) {
                    packed = (pixel[0] >> 3) << 11 | (pixel[1] >> 2) << 5
                        | pixel[2] >> 3;
                    CHECK_EQ_INT(pixel[3], 255);
                } else if (cases[c].depth == IMAGE_DEPTH_A1R5G5B5) {
                    packed = (pixel[3] >> 7) << 15 | (pixel[0] >> 3) << 10
                        | (pixel[1] >> 3) << 5 | pixel[2] >> 3;
                } else {
                    packed = (pixel[3] >> 4) << 12 | (pixel[0] >> 4) << 8
                        | (pixel[1] >> 4) << 4 | pixel[2] >> 4;
                }
                CHECK_EQ_INT(packed, cases[c].pixels[y % 4 * 4 + x % 4]);
            }
        }
        Image_Free(&image);
    }
}

TEST(dithering_preserves_full_precision_and_extreme_channels)
{
    static const IMAGE_DEPTH depths[] = {
        IMAGE_DEPTH_R5G6B5, IMAGE_DEPTH_A1R5G5B5, IMAGE_DEPTH_A4R4G4B4,
        IMAGE_DEPTH_RGBA8,  IMAGE_DEPTH_RGB8,
    };
    const uint8_t black[4] = { 0, 0, 0, 0 };
    const uint8_t white[4] = { 255, 255, 255, 255 };
    for (size_t i = 0; i < sizeof(depths) / sizeof(depths[0]); i++) {
        IMAGE image = M_Make2x2(black, white, white, black);
        Image_QuantizeDithered(&image, depths[i]);
        for (int32_t channel = 0; channel < 3; channel++) {
            CHECK_EQ_INT(image.pixels[channel], 0);
            CHECK_EQ_INT(image.pixels[4 + channel], 255);
        }
        Image_Free(&image);
    }
    const uint8_t color[4] = { 13, 47, 139, 221 };
    IMAGE image = M_Make2x2(color, color, color, color);
    Image_QuantizeDithered(&image, IMAGE_DEPTH_RGBA8);
    for (int32_t i = 0; i < 4; i++) {
        CHECK(memcmp(image.pixels + i * 4, color, 4) == 0);
    }
    Image_QuantizeDithered(&image, IMAGE_DEPTH_RGB8);
    CHECK_EQ_INT(image.pixels[3], 255);
    Image_Free(&image);
}
