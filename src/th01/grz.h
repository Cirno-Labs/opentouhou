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

// Loads HGRX data as 640 by 400 row-major color indices and shared coverage.
// pixels holds stream_count consecutive images. Coverage is zero or one and
// applies to every stream; an opaque color-zero pixel remains covered.
// Uncovered pixels are zero. Expands the four-bit RGB palette to opaque
// eight-bit colors without applying it to drawing state. Supports one
// through 16 streams. Clips runs at the visible image boundary.
// Keeps reader open after the record. Results come from allocator, which is
// meant to be an arena and owns their lifetime. Leaves out empty and
// allocates nothing from allocator on failure.
RESULT Grx_Load(READER *reader, const ALLOCATOR *allocator, GRX *out);

// Loads all HGRX images of an HGRZ container, with the same storage and pixel
// rules as Grx_Load. Supports one through 16 images, including repeated or
// unordered offsets. Offsets are relative to the container's start and must
// lie within its recorded size. Keeps reader open after that size.
// Leaves out empty and allocates nothing from allocator on failure.
RESULT Grz_Load(READER *reader, const ALLOCATOR *allocator, GRZ *out);
