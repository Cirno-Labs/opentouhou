#pragma once

#include "core/io/reader.h"

#define GRF_WIDTH 640
#define GRF_HEIGHT 66

typedef struct {
    uint8_t *coverage;
} GRF;

// Loads the blue plane of the HGRF HUD mask as 640 by 66 coverage values.
RESULT Grf_Load(READER *reader, const ALLOCATOR *allocator, GRF *out);
