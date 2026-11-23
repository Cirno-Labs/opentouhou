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

// Loads HGRX data as stream_count consecutive 640 by 400 row-major images
// of color indices, with coverage of zero or one shared by every stream.
// A covered pixel may hold color zero; uncovered pixels are zero. Expands
// the four-bit RGB palette to opaque eight-bit colors. Supports one through
// 16 streams. Clips runs at the image boundary.
RESULT Grx_Load(READER *reader, const ALLOCATOR *allocator, GRX *out);

// Loads all HGRX images of an HGRZ container as Grx_Load does. Supports one
// through 16 images, including repeated or unordered offsets. Offsets are
// relative to the container's start and must lie within its recorded size.
RESULT Grz_Load(READER *reader, const ALLOCATOR *allocator, GRZ *out);
