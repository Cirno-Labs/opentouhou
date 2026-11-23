#include "th01/pellet_assets.h"

#include "pc98/zun/math16.h"
#include "pc98/zun/sprite_sheet.h"

#include <string.h>

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

static RESULT M_Fill(
    PELLET_ASSETS *const out, const uint8_t *const pellets,
    const size_t pellets_size, const uint8_t *const clouds,
    const size_t clouds_size)
{
    FAIL_IF(pellets_size != 3 * 8, "Invalid pellet.bmp size");
    FAIL_IF(clouds_size != sizeof(out->clouds), "Invalid pellet_c.bmp size");
    for (int32_t image = 0; image < 3; image++) {
        for (int32_t shift = 0; shift < 8; shift++) {
            for (int32_t row = 0; row < 8; row++) {
                const uint16_t value =
                    (uint16_t)((pellets[image * 8 + row] << 8) >> shift);
                out->pellets[image][shift][row][0] = (uint8_t)(value >> 8);
                out->pellets[image][shift][row][1] = (uint8_t)value;
            }
        }
    }
    memcpy(out->clouds, clouds, sizeof(out->clouds));
    return OK;
}

RESULT PelletAssets_Load(
    VFS *const vfs, const ALLOCATOR *const allocator, PELLET_ASSETS **const out)
{
    *out = nullptr;
    uint8_t *pellets;
    size_t pellets_size;
    MUST(SpriteSheet_Load(
        vfs, "pellet.bmp", 8, 8, &g_HeapAllocator, &pellets, &pellets_size));
    uint8_t *clouds = nullptr;
    size_t clouds_size = 0;
    PELLET_ASSETS masks;
    RESULT result = SpriteSheet_Load(
        vfs, "pellet_c.bmp", 16, 16, &g_HeapAllocator, &clouds, &clouds_size);
    if (IS_OK(result)) {
        result = M_Fill(&masks, pellets, pellets_size, clouds, clouds_size);
    }
    Mem_Free(clouds);
    Mem_Free(pellets);
    MUST(result);
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
