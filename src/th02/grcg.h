#pragma once

// The graphics charger: one write paints a color to all four planes at
// once. The one-color shapes (boxes, lines, round boxes) are drawn with it.

#include "th02/clip.h"
#include "th02/video.h"

typedef struct {
    uint8_t mode;
    uint8_t tiles[4];
} GRCG;

extern const GRCG *const g_Charger;

void Grcg_Init(void);
void Grcg_SetColor(uint8_t color);

// Loads one plane's tile byte from VRAM for the next charged draw.
void Grcg_LoadPlane(uint8_t plane, uint8_t byte);

// Sets every plane's tile byte to the same value.
void Grcg_FillTiles(uint8_t tile);

// Enables writes only to the selected B/R/G/I planes.
void Grcg_SetPlanes(uint8_t planes);

// Disables charging, keeping the tile bytes.
void Grcg_Off(void);

// Fills an inclusive box, clipped in signed-word coordinates that wrap.
void Grcg_BoxFill(
    const CLIP *clip, int16_t left, int16_t top, int16_t right, int16_t bottom);

// Draws a horizontal line; unlike BoxFill, X orders after the clip origin.
void Grcg_HLine(const CLIP *clip, int16_t left, int16_t right, int16_t y);

// Draws a clipped vertical line.
void Grcg_VLine(const CLIP *clip, int16_t x, int16_t top, int16_t bottom);

// Draws a clipped line; the clip intersections must fit a signed word.
void Grcg_Line(
    const CLIP *clip, int16_t left, int16_t top, int16_t right, int16_t bottom);

// Fills a box with corners of radius up to half its size; corners wrap
// their signed-word counter, as in retail.
void Grcg_RoundBoxFill(
    const CLIP *clip, int16_t left, int16_t top, int16_t right, int16_t bottom,
    uint16_t radius);
