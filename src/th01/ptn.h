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

// Loads HPTN sprites as count consecutive 32 by 32 row-major images. Keeps
// color index 15, which drawing may treat as transparent. Expands the
// four-bit RGB palette to opaque eight-bit colors. Supports one through 64
// images.
RESULT Ptn_Load(READER *reader, const ALLOCATOR *allocator, PTN *out);
