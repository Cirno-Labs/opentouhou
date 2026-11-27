#pragma once

#include "th02/patterns.h"

#include "core/vfs.h"

typedef struct {
    PATTERN_BANK patterns;
    GFX_COLOR palette[16];
    bool has_palette;
} SIGMA_ASSETS;

// Loads Sigma's banks; the allocator must free individually.
RESULT SigmaAssets_Load(
    VFS *files, const ALLOCATOR *allocator, SIGMA_ASSETS *out);

// Frees the banks once no pattern slot points at them; accepts zeroed storage.
void SigmaAssets_Free(SIGMA_ASSETS *assets);
