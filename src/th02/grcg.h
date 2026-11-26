#pragma once

// The graphics charger (GRCG): one write paints a color to all four planes
// at once, as tiles or as a read-modify-write of the masked bits.

#include "th02/video.h"

typedef struct {
    uint8_t mode;
    uint8_t tiles[4];
} GRCG;

extern const GRCG *const g_Grcg;

void Grcg_Init(void);
void Grcg_SetColor(uint8_t color);

// Loads one plane's tile byte from VRAM for the next charged draw.
void Grcg_LoadTile(uint8_t plane, uint8_t byte);

// Sets every plane's tile byte to the same value.
void Grcg_SetTiles(uint8_t tile);

// Enables writes only to the selected B/R/G/I planes.
void Grcg_SetPlanes(uint8_t planes);

// Disables charging, keeping the tile bytes.
void Grcg_Off(void);

// Writes one byte through the charger: tile mode stores the tiles, RMW
// mode only the masked bits; with charging off, the blue plane gets mask.
void Grcg_WriteByte(uint16_t offset, uint8_t mask);
void Grcg_WriteWord(uint16_t offset, uint16_t mask);
