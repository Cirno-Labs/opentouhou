#pragma once

#include "pc98/zun/pi.h"

#include "core/vfs.h"

typedef struct {
    PI_IMAGE illustration;
    uint8_t particles[5][8];
    uint8_t transition[15][24][3];
    uint8_t text[18][288];
    const ALLOCATOR *allocator;
    uint8_t shot_type;
} BOMB_ASSETS;

// Loads the bomb sprites; the allocator must free individually.
RESULT BombAssets_Load(
    VFS *files, uint8_t shot_type, const ALLOCATOR *allocator,
    BOMB_ASSETS *out);

// Frees the cache once nothing borrows it; accepts a zeroed cache.
void BombAssets_Free(BOMB_ASSETS *assets);
