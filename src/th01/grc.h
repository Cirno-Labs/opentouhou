#pragma once

#include "core/io/reader.h"

#define GRC_MAX_IMAGES 8

typedef struct {
    int32_t width, height, count;
    uint8_t *coverage;
} GRC;

// Loads GRCG monochrome sprites as consecutive row-major coverage images of
// zero or one per pixel; drawing selects the color. Width is a multiple of
// eight. Supports one through eight images and at most 65535 packed bytes
// per image. Discards the file palette, which TH01 does not use for GRC.
RESULT Grc_Load(READER *reader, const ALLOCATOR *allocator, GRC *out);
