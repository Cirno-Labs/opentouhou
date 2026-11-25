#pragma once

#include "core/gfx/types.h"
#include "core/io/reader.h"

#define PTN_WIDTH 32
#define PTN_HEIGHT 32
#define PTN_MAX_IMAGES 64
#define PTN_TRANSPARENT 15

typedef struct {
    int32_t count;
    GFX_COLOR palette[16];
    uint8_t *pixels;
} PTN;

// Loads 1..64 HPTN sprites as 32 by 32 index images, keeping index 15 for
// drawing to treat as transparent.
RESULT Ptn_Load(READER *reader, const ALLOCATOR *allocator, PTN *out);
