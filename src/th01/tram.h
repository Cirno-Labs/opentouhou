#pragma once

#include "th01/text.h"

typedef enum {
    TRAM_TRANSPARENT = -1,
    TRAM_BLACK,
    TRAM_BLUE,
    TRAM_RED,
    TRAM_MAGENTA,
    TRAM_GREEN,
    TRAM_CYAN,
    TRAM_YELLOW,
    TRAM_WHITE,
} TRAM_COLOR;

// Holds an independent 640x400 indexed text layer. Index 0 is transparent;
// indices 1 through 8 are opaque hardware colors in B, R, G bit order.
// Storage belongs to the allocator passed to Init. Keep it alive with the
// context; resetting its arena releases the layer. Only SetVisible changes
// visibility. Drawing never changes graphics pages or their DAC palette.
typedef struct {
    uint8_t *pixels;
    bool visible;
} TRAM;

// Supplies the fixed palette for an indexed overlay texture. Entry 0 has
// zero alpha; every foreground color, including black, has full alpha.
extern const GFX_COLOR g_TramPalette[9];

// Allocates a clear, visible layer. Allocation failure is fatal, as with
// other allocator operations. GPU resources stay with the presenter.
void Tram_Init(TRAM *tram, const ALLOCATOR *allocator);
void Tram_SetVisible(TRAM *tram, bool visible);

// Clears to transparency without changing visibility.
void Tram_Clear(TRAM *tram);

// Replaces a clipped half-open rectangle. Nonpositive sizes draw nothing.
// Colors are TRAM_TRANSPARENT or a hardware color from black through white.
void Tram_Fill(TRAM *tram, GFX_RECT rectangle, TRAM_COLOR color);

// Replaces a cached 8x16 or 16x16 glyph with 16x16 pixels per dot. Set dots
// take foreground and unset dots take background, including transparency.
// Clips to the screen without text-VRAM wrapping. Coordinates may be any
// signed 32-bit value. Glyph rows remain borrowed and unchanged.
void Tram_PutGlyph16(
    TRAM *tram, const GAME_TEXT_GLYPH *glyph, int32_t left, int32_t top,
    TRAM_COLOR foreground, TRAM_COLOR background);

// Centers a line at y=80, clearing the top five text rows and all margins
// through y=335 to background. Requires zero to five cached glyphs totaling
// at most 40 source pixels. An empty line clears that band. Leaves y=336
// through 399 unchanged: stage callers clear
// those rows separately, while HARRY/UP leaves them alone. Five halfwidth
// glyphs fill the screen; two halfwidth glyphs start at x=192.
void Tram_PutLine16(
    TRAM *tram, const GAME_TEXT_GLYPH *const *glyphs, int32_t count,
    TRAM_COLOR foreground, TRAM_COLOR background);

// Fills the layer with opaque black and reveals graphics through a centered
// fullwidth glyph at (192,80). Visibility stays unchanged. Glyph shapes come
// from the cache provider; this operation does not reproduce font ROM data.
void Tram_PutKanji16Reverse(TRAM *tram, const GAME_TEXT_GLYPH *glyph);

// No drawing operation allocates, or reads a renderer, font, clock, file or
// input device. Overlay pixels and palette are ready for the existing core
// indexed-texture API; visibility and uploads belong to the scene presenter.
