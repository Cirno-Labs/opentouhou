#include "th01/pellet_assets.h"

#include "pc98/zun/math16.h"

static void M_Row(
    const uint8_t *const mask, const uint16_t offset, const bool restore,
    const uint8_t color)
{
    for (int32_t byte = 0; byte < 2; byte++) {
        const uint32_t address = ((uint32_t)offset + byte) & 0x7fffu;
        if (address >= 32000) {
            continue;
        }
        for (int32_t bit = 0; bit < 8; bit++) {
            if ((mask[byte] & (0x80u >> bit)) != 0) {
                const uint32_t pixel = address * 8 + bit;
                g_Video.pages[g_Video.drawing_page][pixel] = restore
                    ? (g_Video.pages[g_Video.drawing_page][pixel]
                       | (g_Video.pages[1][pixel]
                          & (offset + byte >= 0x10000      ? 4
                                 : offset + byte >= 0x8000 ? 6
                                                           : 15)))
                    : color;
            }
        }
    }
}

RESULT PelletAssets_Load(
    READER *const reader, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const allocator, PELLET_ASSETS **const out)
{
    *out = nullptr;
    READER source;
    MUST(ReiidenImage_Open(reader, scratch, scratch_size, &source));
    PELLET_ASSETS masks;
    Reader_Seek(&source, 0x398cf);
    Reader_ReadBytes(&source, masks.pellets, sizeof(masks.pellets));
    Reader_Seek(&source, 0x38d9e);
    Reader_ReadBytes(&source, masks.clouds, sizeof(masks.clouds));
    MUST(Reader_Result(&source));
    *out = Allocator_Alloc(allocator, sizeof(masks));
    **out = masks;
    return OK;
}

void PelletAssets_Put(
    const PELLET_ASSETS *const assets, const int16_t left, const int16_t top,
    const int16_t image)
{
    uint16_t offset = (uint16_t)(((uint16_t)left >> 3) + top * 80);
    for (int32_t row = 0; row < 8; row++) {
        M_Row(
            assets->pellets[image][(uint16_t)left & 7][row], offset, false, 7);
        offset = (uint16_t)(offset + 80);
        if (Math16_Wrap(offset) >= 32000) {
            return;
        }
    }
}

void PelletAssets_Cloud(
    const PELLET_ASSETS *const assets, const int16_t left, const int16_t top,
    const int16_t image, const bool restore)
{
    const uint16_t origin = (uint16_t)(left / 8 - (left % 8 < 0) + top * 80);
    uint16_t offset = origin;
    for (int32_t row = 0; row < 16; row++) {
        M_Row(assets->clouds[image][row], offset, false, restore ? 0 : 7);
        offset = (uint16_t)(offset + 80);
    }
    if (restore) {
        offset = origin;
        for (int32_t row = 0; row < 16; row++) {
            const uint8_t *const mask = assets->clouds[image][row];
            if (mask[0] != 0 || mask[1] != 0) {
                Video_SelectDrawingPage(0);
                M_Row(mask, offset, true, 0);
            }
            offset = (uint16_t)(offset + 80);
        }
    }
}
