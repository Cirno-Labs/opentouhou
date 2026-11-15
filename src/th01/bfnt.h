#pragma once

#include "core/gfx/types.h"
#include "core/io/reader.h"

typedef struct {
    int32_t width, height, count;
    GFX_COLOR palette[16];
    uint8_t *pixels;
} BFNT;

// Loads packed 16-color BFNT sprites with an embedded palette and no
// extension records. Index zero is transparent. Pixels come from allocator,
// which is meant to be an arena and owns their lifetime. Keeps reader open.
// Leaves out empty and allocates nothing from allocator on failure.
RESULT Bfnt_Load(READER *reader, const ALLOCATOR *allocator, BFNT *out);
