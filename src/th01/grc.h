#pragma once

#include "core/io/reader.h"

#define GRC_MAX_IMAGES 8

typedef struct {
    int32_t width;
    int32_t height;
    int32_t count;
    uint8_t *coverage;
} GRC;

// Loads 1..8 GRCG monochrome sprites as coverage images; drawing picks the
// color. Discards the file palette.
RESULT Grc_Load(READER *reader, const ALLOCATOR *allocator, GRC *out);
