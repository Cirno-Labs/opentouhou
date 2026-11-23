#pragma once

#include "core/base/memory.h"
#include "core/gfx/types.h"

#define VIDEO_WIDTH 640
#define VIDEO_HEIGHT 400
#define VIDEO_PAGE_SIZE (VIDEO_WIDTH * VIDEO_HEIGHT)

// Holds two packed 640x400 indexed pages and the displayed 16-color DAC.
// Pixel indices hold blue, red, green and intensity in bits 0 through 3.
// Only the page selectors change drawing_page and visible_page.
typedef struct {
    uint8_t *pages[2];
    int32_t drawing_page, visible_page;
    GFX_COLOR palette[16];
    int16_t scroll_top;
} VIDEO;

// Views row-major indices 0 through 15. stride and coverage_stride are the
// byte distances between rows. Optional coverage holds one byte per pixel,
// and zero skips the pixel. key is -1 for opaque indices or the transparent
// index 0 through 15. Coverage and key both apply when present.
typedef struct {
    const uint8_t *pixels, *coverage;
    int32_t width, height, stride, coverage_stride;
    int32_t key;
} VIDEO_IMAGE;

// Views one byte of coverage per pixel, laid out as VIDEO_IMAGE. Nonzero
// bytes draw the supplied color.
typedef struct {
    const uint8_t *pixels;
    int32_t width, height, stride;
} VIDEO_MASK;

// Allocates zeroed pages and selects page 0 with an opaque black DAC.
void Video_Init(VIDEO *video, const ALLOCATOR *allocator);
void Video_SelectDrawingPage(VIDEO *video, int32_t page);
void Video_SelectVisiblePage(VIDEO *video, int32_t page);

// Draws on the drawing page with color masked to four bits, clipped to the
// screen. Line and box endpoints are inclusive and may be reversed.
void Video_Clear(VIDEO *video, uint8_t color);
void Video_Pixel(VIDEO *video, int32_t x, int32_t y, uint8_t color);
uint8_t Video_ReadPixel(const VIDEO *video, int32_t page, int32_t x, int32_t y);
void Video_HorizontalLine(
    VIDEO *video, int32_t left, int32_t right, int32_t y, uint8_t color);
void Video_VerticalLine(
    VIDEO *video, int32_t x, int32_t top, int32_t bottom, uint8_t color);
void Video_FillBox(
    VIDEO *video, int32_t left, int32_t top, int32_t right, int32_t bottom,
    uint8_t color);
void Video_Box(
    VIDEO *video, int32_t left, int32_t top, int32_t right, int32_t bottom,
    uint8_t color);

// Draws with the original's integer-slope clipping and Bresenham rounding.
// Coordinates must fit signed 16 bits. Oblique lines omit their final
// accumulated byte/row group; horizontal and vertical lines include both
// endpoints. Clip differences, integer slopes, interpolation distances and
// cursor arithmetic wrap to signed 16 bits, including raw laser resets. Mask
// writes use 15-bit planar addresses and drop hidden bytes. Patterned lines
// rotate a 16-bit horizontal mask at each cursor, low byte first.
// Horizontal lines ignore the pattern; vertical lines repeat it.
void Video_Line(
    VIDEO *video, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
    uint8_t color);

void Video_LinePatterned(
    VIDEO *video, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t color,
    uint16_t pattern);

// Restores raw four-byte groups from page 1 to page 0, starting one byte
// before each cached cursor. Uses the general cursor even for axis-aligned
// lines and points. The final restore follows the one-past cursor, including
// its off-by-one row. Selects drawing page 0 only when a restore is reached.
// Raw addresses above 32767 alias the visible red/green planes independently;
// hidden and non-VRAM bytes have no storage. Completes byte copies across
// offset 65535, where the original machine may raise a protection fault.
void Video_LineErase(
    VIDEO *video, int32_t x1, int32_t y1, int32_t x2, int32_t y2);

// Draws a half-open source rectangle with its origin at (x, y) on the
// drawing page, clipped to both the source view and the screen.
void Video_Blit(
    VIDEO *video, const VIDEO_IMAGE *image, GFX_RECT source, int32_t x,
    int32_t y);
void Video_Mask(
    VIDEO *video, const VIDEO_MASK *mask, GFX_RECT source, int32_t x, int32_t y,
    uint8_t color);

// Copies between pages with the mapping and clipping of Video_Blit. Overlap
// behaves as if all source pixels were read before any writes.
void Video_Copy(
    VIDEO *video, int32_t source_page, GFX_RECT source,
    int32_t destination_page, int32_t x, int32_t y);

// Restores page 1 to page 0 at the same coordinates. Rounds the left edge
// down to 16 and the right edge up to 16, then clips to the screen. Selects
// drawing page 0, even for an empty rectangle.
void Video_Restore16(VIDEO *video, GFX_RECT rectangle);

// Reads a rectangle of page into rows of stride bytes with the mapping and
// clipping of Video_Blit, leaving the bytes of off-screen pixels unchanged.
void Video_Read(
    const VIDEO *video, int32_t page, GFX_RECT source, uint8_t *pixels,
    int32_t stride);

// Sets a raw DAC entry, masking each component to four bits. Expands each
// nibble by 17 for display and forces alpha to 255, including for index 0.
void Video_SetDac(
    VIDEO *video, int32_t index, int32_t red, int32_t green, int32_t blue);

// Sets the DAC from the upper four bits of each channel, ignoring alpha.
// palette may alias video->palette.
void Video_SetPalette(VIDEO *video, const GFX_COLOR palette[16]);

// Sets the display row origin modulo 400: presentation shows row
// (screen_y + scroll_top) modulo 400 of visible_page.
void Video_SetScroll(VIDEO *video, int16_t top);

// Toggles one plane bit in a byte-aligned 32 by 32 square on the drawing
// page. Returns false without drawing for left < 0, top < 0, left >= 608 or
// top >= 368, where the original rejects the square. Rounds X down to
// eight. plane_bit must be 1, 2, 4 or 8.
bool Video_Invert32(VIDEO *video, int16_t left, int16_t top, uint8_t plane_bit);
