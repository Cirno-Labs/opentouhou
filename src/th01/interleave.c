#include "th01/interleave.h"

#include "core/base/assert.h"

RESULT Interleave_LoadAssets(
    READER *const reader, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const allocator, INTERLEAVE_ASSETS **const out)
{
    *out = nullptr;
    READER source;
    MUST(ReiidenImage_Open(reader, scratch, scratch_size, &source));
    INTERLEAVE_ASSETS masks;
    Reader_Seek(&source, 0x38dec);
    Reader_ReadBytes(&source, masks.masks, sizeof(masks.masks));
    MUST(Reader_Result(&source));
    *out = Allocator_Alloc(allocator, sizeof(masks));
    **out = masks;
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
