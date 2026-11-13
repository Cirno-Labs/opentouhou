#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/base/types.h"

// Decodes PNG, JPEG and BMP into RGBA8, encodes PNG and JPEG, and applies
// what the originals' texture loading did to images. Pixels are always
// stored as 4 bytes, R, G, B, A, row by row with no padding.
//
// Example:
// IMAGE image;
// MUST(Image_Decode(&reader, &g_HeapAllocator, &image));
// Image_ColorKey(&image, 0x000000);
// Image_Free(&image);

typedef struct {
    int32_t width;
    int32_t height;
    uint8_t *pixels;
    const ALLOCATOR *allocator;
} IMAGE;

typedef enum {
    IMAGE_DEPTH_A1R5G5B5,
    IMAGE_DEPTH_A4R4G4B4,
    IMAGE_DEPTH_R5G6B5,
} IMAGE_DEPTH;

// Creates an image of width x height with every pixel 0.
IMAGE Image_Create(int32_t width, int32_t height, const ALLOCATOR *allocator);

void Image_Free(IMAGE *image);

// Sets alpha to 0 on every pixel whose colour is rgb (0xRRGGBB).
void Image_ColorKey(IMAGE *image, uint32_t rgb);

// Sets each pixel's alpha to the blue channel of the same pixel in alpha.
// A size mismatch is a failure, and image is left unchanged.
RESULT Image_AlphaFrom(IMAGE *image, const IMAGE *alpha);

// Rounds every channel to depth, keeping 8-bit storage.
void Image_Quantize(IMAGE *image, IMAGE_DEPTH depth);
