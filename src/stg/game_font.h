#pragma once

#include "core/base/result.h"
#include "core/text/font.h"

// Opens the games' text font at pixel_height: family linked with the default
// font, or the default font alone when family is nullptr or does not open,
// which is logged as a warning. The default font is ＭＳ ゴシック linked with
// MS UI Gothic, as the originals' GDI links them, else IPAGothic linked with
// IPAPGothic sized em to cell, so that lines keep MS Gothic's widths.
// Reports failure when no font opens.
RESULT GameFont_Open(const char *family, float pixel_height, FONT **out);
