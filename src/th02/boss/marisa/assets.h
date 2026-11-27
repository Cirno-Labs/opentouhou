#pragma once

#include "th02/patterns.h"

#include "core/vfs.h"

typedef struct {
    PATTERN_BANK summon;
    PATTERN_BANK combat;
    GFX_COLOR summon_palette[16];
    GFX_COLOR combat_palette[16];
    bool has_summon_palette;
    bool has_combat_palette;
} MARISA_ASSETS;

// Loads Marisa's banks; the allocator must free individually.
RESULT MarisaAssets_Load(
    VFS *files, const ALLOCATOR *allocator, MARISA_ASSETS *out);

// Frees the banks once no pattern slot points at them; accepts zeroed storage.
void MarisaAssets_Free(MARISA_ASSETS *assets);
