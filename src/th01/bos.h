#pragma once

#include "core/io/reader.h"

typedef struct {
    int32_t width, height, count;
    const uint8_t *pixels;
    const uint8_t *coverage;
} BOS;

// Loads BOS sprites as consecutive row-major images. Pixels contain raw
// B,R,G,E indices (0 through 15); coverage is one where the file alpha bit
// is zero. Preserves color outside coverage for the distinct actor blitters.
// Supports byte widths, heights and counts from 1 through 255, including odd
// byte widths; width in pixels is eight times the byte width. Does not apply
// actor slot limits or word-width clipping. Requires BOSS magic as an integrity
// check, though the original ignores it. Ignores reserved fields and discards
// the palette, which the original loaders read or skip without applying.
// Keeps reader open, starts at its current position and leaves trailing data.
// Stores pixels then coverage in one immutable block from allocator, intended
// for an arena that owns its lifetime. Leaves out empty and allocates nothing
// from allocator on failure. Allocation failure is fatal as in core/base.
RESULT Bos_Load(READER *reader, const ALLOCATOR *allocator, BOS *out);
