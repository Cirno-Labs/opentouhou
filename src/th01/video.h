#pragma once

#include "core/base/memory.h"
#include "core/gfx/types.h"

#define VIDEO_WIDTH 640
#define VIDEO_HEIGHT 400
#define VIDEO_PAGE_SIZE (VIDEO_WIDTH * VIDEO_HEIGHT)

// Holds two packed 640x400 indexed pages and the displayed 16-color DAC.
// Pixel indices hold blue, red, green and intensity in bits 0 through 3.
typedef struct {
    uint8_t *pages[2];
    int32_t drawing_page;
    int32_t visible_page;
    GFX_COLOR palette[16];
    int16_t scroll_top;
} VIDEO;

extern VIDEO g_Video;

// Views row-major indices 0 through 15, with optional per-pixel coverage
// (zero skips) and a transparent key index (-1 for none).
typedef struct {
    const uint8_t *pixels;
    const uint8_t *coverage;
    int32_t width;
    int32_t height;
    int32_t stride;
    int32_t coverage_stride;
    int32_t key;
} VIDEO_IMAGE;

// Views one byte of coverage per pixel, laid out as VIDEO_IMAGE. Nonzero
// bytes draw the supplied color.
typedef struct {
    const uint8_t *pixels;
    int32_t width;
    int32_t height;
    int32_t stride;
} VIDEO_MASK;

// Allocates zeroed pages and selects page 0 with an opaque black DAC.
void Video_Init(const ALLOCATOR *allocator);
void Video_SelectDrawingPage(int32_t page);
void Video_SelectVisiblePage(int32_t page);

// Draws on the drawing page with color masked to four bits, clipped to the
// screen. Line and box endpoints are inclusive and may be reversed.
void Video_Clear(uint8_t color);
void Video_Pixel(int32_t x, int32_t y, uint8_t color);
void Video_HorizontalLine(
    int32_t left, int32_t right, int32_t y, uint8_t color);
void Video_VerticalLine(int32_t x, int32_t top, int32_t bottom, uint8_t color);
void Video_FillBox(
    int32_t left, int32_t top, int32_t right, int32_t bottom, uint8_t color);

// Draws a line with the original's clipping, Bresenham rounding and 16-bit
// wraparound; oblique lines omit their final byte/row group.
void Video_Line(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t color);

void Video_LinePatterned(
    int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t color,
    uint16_t pattern);

// Restores the four-byte groups along a line from page 1 to page 0.
// Keeps the original's off-by-one final row and 16-bit address aliasing.
void Video_LineErase(int32_t x1, int32_t y1, int32_t x2, int32_t y2);

// Draws a half-open source rectangle with its origin at (x, y) on the
// drawing page, clipped to both the source view and the screen.
void Video_Blit(
    const VIDEO_IMAGE *image, GFX_RECT source, int32_t x, int32_t y);
void Video_Mask(
    const VIDEO_MASK *mask, GFX_RECT source, int32_t x, int32_t y,
    uint8_t color);

// Copies between pages with the mapping and clipping of Video_Blit. Overlap
// behaves as if all source pixels were read before any writes.
void Video_Copy(
    int32_t source_page, GFX_RECT source, int32_t destination_page, int32_t x,
    int32_t y);

// Restores page 1 to page 0, with the edges widened to multiples of 16.
// Selects drawing page 0, even for an empty rectangle.
void Video_Restore16(GFX_RECT rectangle);

// Reads a rectangle of page into rows of stride bytes with the mapping and
// clipping of Video_Blit, leaving the bytes of off-screen pixels unchanged.
void Video_Read(int32_t page, GFX_RECT source, uint8_t *pixels, int32_t stride);

// Sets a raw DAC entry, masking each component to four bits. Expands each
// nibble by 17 for display and forces alpha to 255, including for index 0.
void Video_SetDac(int32_t index, int32_t red, int32_t green, int32_t blue);

// Sets the DAC from the upper four bits of each channel, ignoring alpha.
// palette may alias g_Video.palette.
void Video_SetPalette(const GFX_COLOR palette[16]);

// Sets the display row origin modulo 400: presentation shows row
// (screen_y + scroll_top) modulo 400 of visible_page.
void Video_SetScroll(int16_t top);

// Toggles plane_bit (1, 2, 4 or 8) in a 32 by 32 square on the drawing page.
// Returns false without drawing outside 0..607 by 0..367; rounds X down to 8.
bool Video_Invert32(int16_t left, int16_t top, uint8_t plane_bit);
