#pragma once

#include "core/gfx/types.h"
#include "core/io/reader.h"

// Decodes ZUN's BFNT glyph and sprite bank format, a strip of
// fixed-size images sharing one palette.

typedef struct {
    int32_t width;
    int32_t height;
    int32_t count;
    uint16_t first; // character code of the first image, for glyph banks
    uint8_t bits_per_pixel; // 4 for sprites, 1 for masks
    bool has_palette;
    GFX_COLOR palette[16];
    const uint8_t *pixels;
} BFNT;

// Loads BFNT images as one byte per pixel, image after image. Four-bit
// sprites may carry a palette and keep index 0 transparent; one-bit masks
// hold 0 and 1.
RESULT Bfnt_Load(READER *reader, const ALLOCATOR *allocator, BFNT *out);
