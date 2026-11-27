#include "th02/bomb/tail.h"

#include "core/base/assert.h"

#include <string.h>

#define M_NEXT_BLOCK 0x72f3
#define M_DEMO_RECORD_PARAGRAPHS 876

void BombTail_Build(
    const TRAM_FONT *const font, const bool demo, uint8_t out[BOMB_TAIL_SIZE])
{
    ASSERT(font->row != nullptr);
    memset(out, 0, BOMB_TAIL_SIZE);
    const uint16_t next =
        demo ? M_NEXT_BLOCK - M_DEMO_RECORD_PARAGRAPHS : M_NEXT_BLOCK;
    out[8] = 1;
    out[10] = (uint8_t)next;
    out[11] = (uint8_t)(next >> 8);
    for (uint8_t y = 0; y < 16; y++) {
        const uint8_t row =
            (uint8_t)(font->row(font->context, 0x56, false, y) >> 8);
        out[24 + y * 2] = out[25 + y * 2] = row;
    }
}
