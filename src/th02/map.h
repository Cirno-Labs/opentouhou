#pragma once

// A stage map: the tile numbers of its sections, each eight rows of 24
// tiles, which the stage chains into its scrolling background.

#include "core/io/reader.h"

#define MAP_SECTION_COUNT 16
#define MAP_SECTION_ROWS 8
#define MAP_COLUMNS 24

typedef struct {
    uint8_t tiles[MAP_SECTION_ROWS][MAP_COLUMNS];
} MAP_SECTION;

typedef struct {
    const MAP_SECTION *sections;
} MAP;

// Loads a map, skipping its unchecked ten-byte header.
RESULT Map_Load(READER *reader, const ALLOCATOR *allocator, MAP *out);
