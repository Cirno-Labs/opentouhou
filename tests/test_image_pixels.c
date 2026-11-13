// SPDX-License-Identifier: GPL-3.0-only

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
    // 0x3f truncates to the top 4 bits, 0x3, which replicates to 0x33.
    CHECK_EQ_INT(image.pixels[0], 0x33);
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
