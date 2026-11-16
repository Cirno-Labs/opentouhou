#pragma once

#include "core/gfx/types.h"
#include "core/io/reader.h"

typedef struct {
    int32_t width, height;
    int32_t x, y;
    int32_t transparent;
    bool has_position, has_palette;
    GFX_COLOR palette[16];
    uint8_t *pixels;
} PI_IMAGE;

// Loads a four-plane Pi image or TH01's ZN variant as row-major indices
// from 0 to 15. Supports dimensions from 2 by 1 through 1280 by 800.
// Expands ZN's four-bit RGB components to eight bits; Pi keeps its eight-bit
// components. Palette entries are opaque. transparent is the embedded
// color key, or -1, so drawing can apply or override it independently.
// A file without a palette uses the producer's complete default palette.
// x and y hold unsigned embedded coordinates, or zero without a position.
// Stops at an unknown extension record. Leaves out empty on failure.
// Does not close reader. Pixels come from allocator, which is meant to be
// an arena and owns their lifetime. Leaves out empty and allocates nothing
// from allocator on failure.
RESULT Pi_Load(READER *reader, const ALLOCATOR *allocator, PI_IMAGE *out);
