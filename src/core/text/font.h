#pragma once

#include "core/base/result.h"
#include "core/io/reader.h"

typedef struct TEXT_FONT TEXT_FONT;

// Initializes font rasterization. Free every font before shutdown.
RESULT Text_Init(void);
void Text_Shutdown(void);

// Reads the remaining font data and opens it at the given pixel height.
// Owns a copy of the data, so the reader can close after this call.
// Leaves out empty on failure. Use fonts from the main thread.
RESULT TextFont_Load(READER *reader, float pixel_height, TEXT_FONT **out);

// Opens the bundled BIZ UDGothic Bold font under the SIL Open Font License.
RESULT TextFont_LoadDefault(float pixel_height, TEXT_FONT **out);
void TextFont_Free(TEXT_FONT *font);
