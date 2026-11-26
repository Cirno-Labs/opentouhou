#include "th02/gaiji.h"

RESULT Gaiji_Register(GAIJI *const bank, const BFNT *const font)
{
    FAIL_IF(
        font->width != 16 || font->height != 16 || font->count != 256
            || font->first != 0 || font->bits_per_pixel != 1
            || font->has_palette || font->pixels == nullptr,
        "Invalid gaiji font");
    for (size_t pixel = 0; pixel < 256 * 16 * 16; pixel++) {
        FAIL_IF(font->pixels[pixel] > 1, "Invalid gaiji mask pixel");
    }
    for (size_t glyph = 0; glyph < 256; glyph++) {
        for (size_t y = 0; y < 16; y++) {
            uint16_t row = 0;
            for (size_t x = 0; x < 16; x++) {
                row |= (uint16_t)font->pixels[glyph * 256 + y * 16 + x]
                    << (15 - x);
            }
            bank->rows[glyph][y] = row;
        }
    }
    return OK;
}
