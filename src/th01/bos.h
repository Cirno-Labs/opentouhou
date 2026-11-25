#pragma once

#include "core/io/reader.h"

typedef struct {
    int32_t width, height, count;
    const uint8_t *pixels;
    const uint8_t *coverage;
} BOS;

// Loads 1..255 BOS sprites as raw B,R,G,E index images, with coverage where
// the file alpha bit is zero. Keeps the color outside coverage.
RESULT Bos_Load(READER *reader, const ALLOCATOR *allocator, BOS *out);
