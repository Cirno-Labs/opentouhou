#pragma once

// MPN tile sheets: opaque 16x16 tiles with a palette of their own. Stages
// lay their background out of them, and the dialogue cuts its faces from
// them. Unlike pattern banks, which hold sprites of any size with
// transparency, a sheet's tiles are all one size and always drawn whole.

#include "core/gfx/types.h"
#include "core/io/reader.h"

#define MPN_TILE_WIDTH 16
#define MPN_TILE_HEIGHT 16

typedef struct {
    uint16_t count;
    GFX_COLOR palette[16];
    uint8_t *pixels;
} MPN;

// Loads an MPN's indexed tiles and palette.
RESULT Mpn_Load(READER *reader, const ALLOCATOR *allocator, MPN *out);
