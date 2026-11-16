#pragma once

#include "core/base/memory.h"
#include "core/gfx/types.h"

#define VIDEO_WIDTH 640
#define VIDEO_HEIGHT 400
#define VIDEO_PAGE_SIZE (VIDEO_WIDTH * VIDEO_HEIGHT)

// Holds two tightly packed indexed pages and the displayed 16-color DAC.
// Allocate storage with Video_Init from a caller-owned allocator. Keep that
// storage alive with the context; resetting its arena releases both pages.
// Pixel indices use blue, red, green and intensity as bits 0 through 3.
// Only the page selectors may change the draw and visible page fields.
// Direct page writes are allowed; presentation uploads are managed outside
// this module. No operation reads a renderer, clock, file or input device.
typedef struct {
    uint8_t *pages[2];
    int32_t drawing_page, visible_page;
    GFX_COLOR palette[16];
} VIDEO;

// Views immutable row-major indices. stride and coverage_stride are byte
// distances between rows and must be at least width. Optional coverage uses
// one byte per pixel: zero skips the pixel, any other value covers it.
// key is -1 for opaque indices or 0 through 15 for a transparent color.
// Coverage and key both apply when present. Indices must be 0 through 15.
// The storage must cover every declared row and must not alias video pages.
typedef struct {
    const uint8_t *pixels, *coverage;
    int32_t width, height, stride, coverage_stride;
    int32_t key;
} VIDEO_IMAGE;

// Views one byte of coverage per pixel with the same row and lifetime rules
// as VIDEO_IMAGE. Nonzero bytes draw the supplied solid color.
typedef struct {
    const uint8_t *pixels;
    int32_t width, height, stride;
} VIDEO_MASK;

// Allocates zeroed pages and initializes an opaque black DAC and page 0.
// Allocation failure is fatal, as with the other allocator operations.
void Video_Init(VIDEO *video, const ALLOCATOR *allocator);
void Video_SelectDrawingPage(VIDEO *video, int32_t page);
void Video_SelectVisiblePage(VIDEO *video, int32_t page);

// Draw on the selected drawing page. Mask color to four bits. Clip writes
// to the screen. Line and box endpoints are inclusive and may be reversed.
// ReadPixel returns zero for coordinates outside the screen.
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

// Reproduce the source's integer-slope clipping and Bresenham rounding.
// Coordinates must fit signed 16 bits. Oblique lines omit their final
// accumulated byte/row group; horizontal and vertical lines include both
// endpoints. Out-of-screen writes are discarded, with no VRAM aliasing.
void Video_Line(
    VIDEO *video, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
    uint8_t color);

// Use half-open GFX_RECT bounds; nonpositive dimensions draw no pixels.
// Map the source rectangle's origin to (x, y) on the drawing page. Clip to
// both the source view and the screen, preserving that mapping.
void Video_Blit(
    VIDEO *video, const VIDEO_IMAGE *image, GFX_RECT source, int32_t x,
    int32_t y);
void Video_Mask(
    VIDEO *video, const VIDEO_MASK *mask, GFX_RECT source, int32_t x, int32_t y,
    uint8_t color);

// Copy between arbitrary pages with the same mapping and clipping rules.
// Overlap behaves as if all source pixels were read before any writes.
// Leave both page selectors unchanged.
void Video_Copy(
    VIDEO *video, int32_t source_page, GFX_RECT source,
    int32_t destination_page, int32_t x, int32_t y);

// Restore page 1 to page 0 at the same coordinates. Round the left edge
// down to 16 and the original right edge up to 16, then clip to the screen.
// Select drawing page 0 even for an empty rectangle.
void Video_Restore16(VIDEO *video, GFX_RECT rectangle);

// Read into externally owned rows. stride must cover source.width bytes.
// Preserve the source-to-output mapping when clipping; leave output bytes
// corresponding to out-of-screen pixels unchanged. Storage must not alias
// video pages and must cover source.height rows. Leave selectors unchanged.
void Video_Read(
    const VIDEO *video, int32_t page, GFX_RECT source, uint8_t *pixels,
    int32_t stride);

// Set a raw DAC entry, masking each signed component to four bits. Expand
// each nibble by 17 for display and force alpha to 255, including index 0.
// This changes no scene's logical or saved palette.
void Video_SetDac(
    VIDEO *video, int32_t index, int32_t red, int32_t green, int32_t blue);

// Quantize the upper four bits of each 8-bit resource channel to the DAC.
// Ignore resource alpha. Palette storage may alias video->palette.
void Video_SetPalette(VIDEO *video, const GFX_COLOR palette[16]);
