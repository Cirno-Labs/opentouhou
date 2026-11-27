#pragma once

// The PC-98 graphics VRAM: two 640x400 pages of four bit planes, an access
// page for drawing and a display page for the screen, with the hidden bytes
// past row 400 kept. docs/PC98.md describes the model.

#include "core/base/memory.h"
#include "core/gfx/types.h"
#include "th02/clip.h"

#define VIDEO_WIDTH 640
#define VIDEO_HEIGHT 400
#define VIDEO_VISIBLE_SIZE (VIDEO_WIDTH * VIDEO_HEIGHT)
#define VIDEO_PLANE_SIZE 32768
#define VIDEO_PAGE_SIZE (VIDEO_PLANE_SIZE * 8)

// VRAM with hidden tails; index bits are blue, red, green and intensity.
typedef struct {
    uint8_t *pages[2];
    uint8_t drawing_page;
    uint8_t visible_page;
    uint16_t first_lines;
    uint16_t first_address;
    uint16_t second_address;
    GFX_COLOR palette[16];
} VIDEO;

extern VIDEO g_Video;

// Allocates the pages, which live until shutdown.
void Video_Init(const ALLOCATOR *allocator);

// Selects the drawing page by its low bit.
void Video_AccessPage(uint8_t page);
void Video_ShowPage(uint8_t page);

// Clears only the drawing page's visible area.
void Video_Clear(void);

// Copies the other page into destination and selects it for drawing.
void Video_CopyPage(uint8_t destination);

// Restores a rectangle from page 1 to page 0, X aligned to 8 and width to
// 16 pixels; addresses wrap instead of clipping.
void Video_RestoreRect16(
    int16_t left, int16_t top, int16_t width, int16_t height);

// Draws an image with X and width floored to 8 and rows wrapping at 400,
// clipped only vertically. X >= 0; width even, <= 32766.
void Video_PutImage8(
    const CLIP *clip, const uint8_t *pixels, int16_t width, int16_t height,
    int16_t left, int16_t top);

uint8_t Video_ReadPlaneByte(uint8_t page, uint8_t plane, uint16_t offset);
void Video_WritePlaneByte(uint8_t plane, uint16_t offset, uint8_t bits);

// Splits the display at word addresses; the first region may be empty.
void Video_SetSplit(
    uint16_t first_lines, uint16_t first_address, uint16_t second_address);

// Scrolls the display up to line, at most 400.
void Video_ScrollUp(uint16_t line);

// Renders the visible pages, wrapping within each plane.
void Video_Scanout(uint8_t *pixels, size_t stride);

// Sets a DAC color from 4-bit channels.
void Video_SetDac(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);
