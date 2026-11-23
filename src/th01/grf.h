#pragma once

#include "core/io/reader.h"

#define GRF_WIDTH 640
#define GRF_HEIGHT 66

typedef struct {
    uint8_t *coverage;
} GRF;

// Loads the blue plane of TH01's HGRF HUD mask as 640 by 66 row-major
// coverage values of zero or one. Ignores the palette and the other planes,
// and reads a blue plane whose size runs past the file up to its end. Stops
// at the HUD boundary, also inside a run.
RESULT Grf_Load(READER *reader, const ALLOCATOR *allocator, GRF *out);
