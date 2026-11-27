#pragma once

#include "th02/entity.h"
#include "th02/map.h"
#include "th02/mpn.h"

#define TILE_COLUMNS 24
#define TILE_ROWS 25

typedef enum {
    TILE_BLACK,
    TILE_IMAGES,
    TILE_DISABLED,
} TILE_MODE;

typedef struct {
    uint8_t mode;
    uint8_t images[TILE_ROWS][TILE_COLUMNS];
    uint8_t reserved;
    uint8_t dirty[TILE_COLUMNS][TILE_ROWS];
    uint8_t columns[TILE_COLUMNS];
    uint8_t redraw_all;
} TILE_CACHE;

#define TILE_SOURCE_COUNT 100

typedef struct {
    uint16_t map_row;
    int8_t line;
    int16_t copy_top;
    int16_t copy_height;
    uint8_t row[TILE_COLUMNS];
} TILE_SCROLL;

typedef struct {
    const MAP *map;
    const uint8_t *order;
    uint16_t count;
} TILE_SCROLL_MAP;

typedef struct {
    TILE_CACHE cache;
    TILE_SCROLL scroll;
    TILE_SCROLL_MAP map;
    uint16_t sources[TILE_SOURCE_COUNT];
    uint8_t last_source;
} BACKGROUND;

extern const BACKGROUND *const g_Background;

// Clears the whole background, once per run.
void Background_Init(void);

// Sets the cache's mode, keeping its dirty state.
void Background_SetMode(TILE_MODE mode);

// Marks every tile dirty for the next RepaintDirty.
void Background_MarkAll(void);

// Sets image mode and clears only redraw_all.
void Background_Reset(void);

// Marks the tiles under a rectangle dirty. Like retail, indices in
// [-602,624] past the array alias the adjacent fields; the wrapped bottom
// must stay below 32752.
void Background_Mark(
    int16_t scroll, int16_t left, int16_t top, int16_t width, int16_t height);

// Redraws dirty tiles, or all when redraw_all is exactly one, then clears
// the flags, even in disabled mode.
void Background_RepaintDirty(void);

// Redraws the playfield from the cache in image mode, else clears it.
void Background_RepaintAll(void);

// Restores tile intensity, keeping destination bits 0..2; requires image or
// disabled mode.
void Background_RestoreIntensity(void);

// Returns the source of an image: IDs 0..99 are stage tiles; higher IDs
// read the copy parameters, pending row and cache bytes, as retail does.
uint16_t Background_Source(uint8_t image);

// Sets a tile in the ring at index [-1,sizeof(TILE_CACHE)-2] and draws it
// on front then back, even for equal pages; sources resolve after the write.
void Background_SetTile(
    uint8_t front, uint8_t back, int16_t left, int16_t top, uint8_t image);

// Sets the scrolled map's sections, order and count.
void Background_SetMap(const MAP *map, const uint8_t *order, uint16_t count);

// Sets map row 23 and line zero, keeping the copy parameters and the
// pending row.
void Background_ResetScroll(void);

// Copies one scrolled row at speed pixels; returns true at the map's end.
// The last image at X=16 takes copy_height's high byte, as retail does.
bool Background_Scroll(PLAYFIELD_PAGES pages, uint8_t speed);

// Writes all 100 sources, blank past the loaded count, where the original
// copies whatever follows its tile buffer; requires disabled charging.
void Background_SetSources(const MPN *tiles);

// Draws the first screen of tiles without clipping; requires at least three
// map-order entries.
void Background_DrawInitial(const MPN *tiles);

// Shows a stage's first screen: fills the cache from the map, copies it
// onto page 1 directly, then redraws it from the cache onto page 0.
void Background_ShowStage(void);
