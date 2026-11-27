#pragma once

#include "core/gfx/types.h"
#include "core/io/reader.h"

#define GRX_WIDTH 640
#define GRX_HEIGHT 400
#define GRX_MAX_STREAMS 16
#define GRZ_MAX_IMAGES 16

typedef struct {
    int32_t stream_count;
    GFX_COLOR palette[16];
    uint8_t *coverage;
    uint8_t *pixels;
} GRX;

typedef struct {
    int32_t count;
    GRX images[GRZ_MAX_IMAGES];
} GRZ;

// Loads all 1..16 HGRX images of an HGRZ container, each as stream_count
// (1..16) 640 by 400 index images sharing one coverage; the palette
// expands to opaque eight-bit colors.
RESULT Grz_Load(READER *reader, const ALLOCATOR *allocator, GRZ *out);
