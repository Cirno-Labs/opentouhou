#pragma once

#include "core/base/result.h"
#include "core/io/reader.h"

typedef struct TEXT_FONT TEXT_FONT;

// Initializes font rasterization. Free every font before shutdown.
RESULT Text_Init(void);
void Text_Shutdown(void);

// Font sizes are cell heights, as GDI takes a positive font height: the
// Windows ascent and descent of the OS/2 table together fill pixel_height.
// A font without these metrics takes pixel_height as its em size.

// Reads the remaining font data and opens it at the given pixel height.
// Owns a copy of the data, so the reader can close after this call.
// Leaves out empty on failure. Use fonts from the main thread.
RESULT TextFont_Load(READER *reader, float pixel_height, TEXT_FONT **out);

// Opens the bundled BIZ UDGothic Bold font under the SIL Open Font License.
RESULT TextFont_LoadDefault(float pixel_height, TEXT_FONT **out);
// Opens the first file of paths whose family name is family, ignoring ASCII
// case, at the given pixel height. The bundled font draws the characters
// that the file lacks. A file that does not open is left out, with a log
// line. Reports failure when no file has the family.
RESULT TextFont_LoadFamily(
    const char *const *paths, int32_t count, const char *family,
    float pixel_height, TEXT_FONT **out);
// Changes the size from loaded font data without reading files.
RESULT TextFont_SetSize(TEXT_FONT *font, float pixel_height);
void TextFont_Free(TEXT_FONT *font);
