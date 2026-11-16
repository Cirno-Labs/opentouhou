#pragma once

#include "core/io/reader.h"

#define GRF_WIDTH 640
#define GRF_HEIGHT 66

typedef struct {
    uint8_t *coverage;
} GRF;

// Loads the blue plane of TH01's HGRF HUD mask as 640 by 66 row-major
// coverage values, each zero or one. Ignores the palette and other planes,
// but validates their declared ranges. Stops at the HUD boundary, including
// inside a run. Keeps reader open at the end of the blue plane's stream.
// Coverage comes from allocator, which is meant to be an arena and owns
// its lifetime. Leaves out empty and allocates nothing from allocator on
// failure.
RESULT Grf_Load(READER *reader, const ALLOCATOR *allocator, GRF *out);
