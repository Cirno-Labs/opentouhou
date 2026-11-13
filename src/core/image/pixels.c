// SPDX-License-Identifier: GPL-3.0-only

#include "core/image.h"

// Returns the number of bytes a width x height RGBA8 image occupies.
static size_t M_PixelsSize(const int32_t width, const int32_t height)
{
    return (size_t)width * (size_t)height * 4;
}

// Repeats the top bits bits times until every bit of an 8-bit value is
// filled, the way hardware texture samplers expand a low bit depth channel
// back to 8 bits.
static uint8_t M_Replicate(const uint32_t value, const int bits)
{
    uint32_t result = value;
    int filled = bits;
    while (filled < 8) {
        result = (result << bits) | value;
        filled += bits;
    }
    return (uint8_t)(result >> (filled - 8));
}

// Rounds an 8-bit channel down to bits bits and expands it back to 8 bits
// by bit replication.
static uint8_t M_QuantizeChannel(const uint8_t value, const int bits)
{
    return M_Replicate(value >> (8 - bits), bits);
}

IMAGE Image_Create(
    const int32_t width, const int32_t height, const ALLOCATOR *const allocator)
{
    const size_t size = M_PixelsSize(width, height);
    return (IMAGE) {
        .width = width,
        .height = height,
        .pixels = size == 0 ? nullptr : Allocator_AllocZ(allocator, size),
        .allocator = allocator,
    };
}

void Image_Free(IMAGE *const image)
{
    Allocator_Free(
        image->allocator, image->pixels,
        M_PixelsSize(image->width, image->height));
    *image = (IMAGE) {};
}

void Image_ColorKey(IMAGE *const image, const uint32_t rgb)
{
    const uint8_t key_r = (uint8_t)(rgb >> 16);
    const uint8_t key_g = (uint8_t)(rgb >> 8);
    const uint8_t key_b = (uint8_t)(rgb >> 0);
    const int64_t count = (int64_t)image->width * image->height;
    for (int64_t i = 0; i < count; i++) {
        uint8_t *const pixel = &image->pixels[i * 4];
        if (pixel[0] == key_r && pixel[1] == key_g && pixel[2] == key_b) {
            pixel[3] = 0;
        }
    }
}

RESULT Image_AlphaFrom(IMAGE *const image, const IMAGE *const alpha)
{
    FAIL_IF(
        image->width != alpha->width || image->height != alpha->height,
        "alpha image is %dx%d, expected %dx%d", alpha->width, alpha->height,
        image->width, image->height);
    const int64_t count = (int64_t)image->width * image->height;
    for (int64_t i = 0; i < count; i++) {
        image->pixels[i * 4 + 3] = alpha->pixels[i * 4 + 2];
    }
    return OK;
}

void Image_Quantize(IMAGE *const image, const IMAGE_DEPTH depth)
{
    const int64_t count = (int64_t)image->width * image->height;
    for (int64_t i = 0; i < count; i++) {
        uint8_t *const pixel = &image->pixels[i * 4];
        switch (depth) {
        case IMAGE_DEPTH_A1R5G5B5:
            pixel[0] = M_QuantizeChannel(pixel[0], 5);
            pixel[1] = M_QuantizeChannel(pixel[1], 5);
            pixel[2] = M_QuantizeChannel(pixel[2], 5);
            pixel[3] = M_QuantizeChannel(pixel[3], 1);
            break;
        case IMAGE_DEPTH_A4R4G4B4:
            pixel[0] = M_QuantizeChannel(pixel[0], 4);
            pixel[1] = M_QuantizeChannel(pixel[1], 4);
            pixel[2] = M_QuantizeChannel(pixel[2], 4);
            pixel[3] = M_QuantizeChannel(pixel[3], 4);
            break;
        case IMAGE_DEPTH_R5G6B5:
            pixel[0] = M_QuantizeChannel(pixel[0], 5);
            pixel[1] = M_QuantizeChannel(pixel[1], 6);
            pixel[2] = M_QuantizeChannel(pixel[2], 5);
            pixel[3] = 255;
            break;
        }
    }
}
