#pragma once

#include "core/gfx/types.h"
#include "core/io/reader.h"

typedef struct {
    int32_t width, height, count;
    GFX_COLOR palette[16];
    uint8_t *pixels;
} BFNT;

// Loads packed 16-color BFNT sprites with an embedded palette and no
// extension records. Index zero is transparent.
RESULT Bfnt_Load(READER *reader, const ALLOCATOR *allocator, BFNT *out);
