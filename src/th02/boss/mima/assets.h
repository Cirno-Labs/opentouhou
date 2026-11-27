#pragma once

#include "th02/patterns.h"
#include "core/vfs.h"

typedef enum {
    MIMA_BANK_SUMMON,
    MIMA_BANK_FIRST,
    MIMA_BANK_SECOND,
    MIMA_BANK_COUNT,
} MIMA_BANK;

typedef struct {
    PATTERN_BANK patterns[MIMA_BANK_COUNT];
    GFX_COLOR palettes[MIMA_BANK_COUNT][16];
    bool has_palette[MIMA_BANK_COUNT];
} MIMA_ASSETS;

// Loads Mima's three banks; the allocator must free individually.
RESULT MimaAssets_Load(
    VFS *files, const ALLOCATOR *allocator, MIMA_ASSETS *out);

// Frees the banks once no pattern slot points at them; accepts zeroed storage.
void MimaAssets_Free(MIMA_ASSETS *assets);
