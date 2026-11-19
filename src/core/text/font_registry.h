#pragma once

#include "core/base/result.h"
#include "core/text/font.h"

#include <stddef.h>

// Holds the fonts that the process can open by family name, as GDI's font
// table does.
//
// Names match the family (1), full name (4) and typographic family (16)
// records of a font's name table, in every language, ignoring ASCII case,
// so "MS Gothic" and "ＭＳ ゴシック" both open the face of msgothic.ttc that
// has them. A family opens its Regular face. Use the registry from the main
// thread.

// Adds the font in data, a copy of it, under every family and full name in
// its name table, in any language. For a name added twice, the later font
// wins. source names it in messages.
RESULT FontRegistry_Add(const char *source, const void *data, size_t size);

// Adds the system's installed font that has family, as FontRegistry_Add
// does, except that its names leave the added fonts' names to them. Reports
// failure when the system has none; a substitute the system offers does not
// count. backend_windows.c, backend_macos.c and
// backend_fontconfig.c implement it.
RESULT FontRegistry_AddSystemFont(const char *family);

// Forgets every added font. With use_system, a family that no added font
// has is looked up among the system's installed fonts; without, it is not.
void FontRegistry_Reset(bool use_system);

// Opens the font of family at pixel_height, choosing the matching face of a
// collection: an added font, else, when Reset allowed it, the system's.
// Reports failure when neither has it. The system is asked once for each
// family, and its failure is logged as information.
RESULT FontRegistry_Open(const char *family, float pixel_height, FONT **out);
