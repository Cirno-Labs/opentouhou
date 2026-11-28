#pragma once

#include "core/base/result.h"
#include "core/base/types.h"

// Rasterizes text with a loaded font. A font opens through
// FontRegistry_Open; use fonts from the main thread.

typedef struct FONT FONT;

// Initializes font rasterization. Free every font before shutdown.
RESULT Text_Init(void);
void Text_Shutdown(void);

// Font sizes are cell heights, as GDI takes a positive font height: the
// Windows ascent and descent of the OS/2 table together fill pixel_height.
// A font without these metrics takes pixel_height as its em size.

// Changes the size from loaded font data without reading files.
RESULT Font_SetSize(FONT *font, float pixel_height);

// Rounds each face's em size to whole pixels for the given cell height.
// Gives the image's vertical offset for a baseline rounded from the font's
// Windows ascent, as GDI places the glyphs. Leaves fractional sizing to
// Font_SetSize.
RESULT Font_SetSizeRounded(FONT *font, float pixel_height, int32_t *offset_y);

// Draws the characters that font lacks with fallback, which font then owns,
// at font's em size, as GDI's font linking does. Replaces and frees the
// fallback that font had.
void Font_SetFallback(FONT *font, FONT *fallback);

// Sizes the em of font to its cell height from now on, keeping its size,
// as fonts whose Windows ascent and descent add up to their em are sized.
RESULT Font_SetEmToCell(FONT *font);

// Frees font and its fallback.
void Font_Free(FONT *font);
