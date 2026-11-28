#pragma once

#include "core/gfx/types.h"
#include "core/io/reader.h"

// Decodes ZUN's PI picture format: one indexed image with an optional
// position and transparent index.

typedef struct {
    int32_t width;
    int32_t height;
    int32_t x;
    int32_t y;
    int32_t transparent;
    bool has_position;
    bool has_palette;
    GFX_COLOR palette[16];
    uint8_t *pixels;
} PI_IMAGE;

// Loads a 16-color Pi image, or th01's ZN variant, as row-major indices.
// transparent is the embedded color key or -1; x and y are zero without a
// position, and a file without a palette gets the producer's default
// palette. Stops at an unknown extension record.
RESULT Pi_Load(READER *reader, const ALLOCATOR *allocator, PI_IMAGE *out);
