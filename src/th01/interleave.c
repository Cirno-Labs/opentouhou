#include "th01/interleave.h"

#include "th01/sprite_sheet.h"

#include "core/base/assert.h"

RESULT Interleave_LoadAssets(
    VFS *const vfs, const ALLOCATOR *const allocator,
    INTERLEAVE_ASSETS **const out)
{
    *out = nullptr;
    uint8_t *masks;
    size_t size;
    MUST(SpriteSheet_Load(vfs, "ileave_m.bmp", 8, 8, allocator, &masks, &size));
    FAIL_IF(size != sizeof(INTERLEAVE_ASSETS), "Invalid ileave_m.bmp size");
    *out = (INTERLEAVE_ASSETS *)masks;
    return OK;
}

void Interleave_Put8(
    const INTERLEAVE_ASSETS *const assets, const int16_t left,
    const int16_t top, const uint8_t mask_id)
{
    ASSERT(
        left >= 0 && left < VIDEO_WIDTH && top >= 0 && top <= VIDEO_HEIGHT - 8);
    ASSERT(mask_id < INTERLEAVE_MASK_COUNT);
    const int32_t x = left / 8 * 8;
    for (int32_t y = 0; y < 8; y++) {
        const uint8_t mask = assets->masks[mask_id][y];
        for (int32_t bit = 0; bit < 8; bit++) {
            if ((mask & (0x80u >> bit)) == 0) {
                const int32_t offset = (top + y) * VIDEO_WIDTH + x + bit;
                g_Video.pages[0][offset] = g_Video.pages[1][offset];
            }
        }
    }
    Video_SelectDrawingPage(0);
}
