#pragma once

#include "th02/background/background.h"
#include "th02/mpn.h"

#include "core/io/reader.h"
#include "core/vfs.h"

typedef struct {
    MAP map;
    MPN tiles;
    ALLOCATOR allocator;
} BACKGROUND_ASSETS;

// Loads a map, skipping its unchecked ten-byte header.
RESULT BackgroundAssets_LoadMap(
    READER *reader, const ALLOCATOR *allocator, MAP *out);

// Loads stage N's map and tiles; the allocator must free individually.
RESULT BackgroundAssets_Load(
    VFS *files, uint8_t stage, const ALLOCATOR *allocator,
    BACKGROUND_ASSETS *out);

// Frees the assets once nothing borrows them.
void BackgroundAssets_Free(BACKGROUND_ASSETS *assets);
