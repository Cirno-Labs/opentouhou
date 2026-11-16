#pragma once

#include "core/io/reader.h"

#define GRC_MAX_IMAGES 8

typedef struct {
    int32_t width, height, count;
    uint8_t *coverage;
} GRC;

// Loads GRCG monochrome sprites as consecutive row-major coverage images.
// Each pixel is zero or one; drawing selects the color. Width is a multiple
// of eight. Supports one through eight images and at most 65535 packed bytes
// per image. Discards the file palette, which TH01 does not use for GRC.
// Keeps reader open. Coverage comes from allocator, which is meant to be an
// arena and owns its lifetime. Leaves out empty and allocates nothing from
// allocator on failure.
RESULT Grc_Load(READER *reader, const ALLOCATOR *allocator, GRC *out);
