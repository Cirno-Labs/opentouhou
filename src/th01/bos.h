#pragma once

#include "core/io/reader.h"

typedef struct {
    int32_t width, height, count;
    const uint8_t *pixels;
    const uint8_t *coverage;
} BOS;

// Loads BOS sprites as consecutive row-major images of raw B,R,G,E indices,
// with coverage of one where the file alpha bit is zero. Keeps the color
// outside coverage, which some actor blitters draw. Supports byte widths,
// heights and counts from 1 through 255, including odd byte widths; the
// width in pixels is eight times the byte width. Requires the BOSS magic,
// which the original ignores. Discards the palette, as the original does.
RESULT Bos_Load(READER *reader, const ALLOCATOR *allocator, BOS *out);
