#pragma once

#include "core/base/result.h"
#include "core/io/reader.h"

typedef struct FONT FONT;

// Initializes font rasterization. Free every font before shutdown.
RESULT Text_Init(void);
void Text_Shutdown(void);

// Font sizes are cell heights, as GDI takes a positive font height: the
// Windows ascent and descent of the OS/2 table together fill pixel_height.
// A font without these metrics takes pixel_height as its em size.

// Reads the remaining font data and opens it at the given pixel height.
// Owns a copy of the data, so the reader can close after this call.
// Leaves out empty on failure. Use fonts from the main thread.
RESULT Font_Load(READER *reader, float pixel_height, FONT **out);

// Opens the bundled BIZ UDGothic Bold font under the SIL Open Font License.
RESULT Font_LoadDefault(float pixel_height, FONT **out);

// Opens the first file of paths whose family name is family, ignoring ASCII
// case, at the given pixel height. The bundled font draws the characters
// that the file lacks, at the file's em size. A file that does not open is
// left out, with a log line. Reports failure when no file has the family.
RESULT Font_LoadFamily(
    const char *const *paths, int32_t count, const char *family,
    float pixel_height, FONT **out);

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
