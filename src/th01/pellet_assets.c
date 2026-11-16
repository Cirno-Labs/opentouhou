#include "th01/pellet_assets.h"

#include "th01/math.h"

#include "core/hash/sha256.h"

#include <string.h>

static void M_Row(
    VIDEO *const video, const uint8_t *const mask, const uint16_t offset,
    const bool restore)
{
    for (int32_t byte = 0; byte < 2; byte++) {
        const uint32_t address = ((uint32_t)offset + byte) & 0x7fffu;
        if (address >= 32000) {
            continue;
        }
        for (int32_t bit = 0; bit < 8; bit++) {
            if ((mask[byte] & (0x80u >> bit)) != 0) {
                const uint32_t pixel = address * 8 + bit;
                video->pages[video->drawing_page][pixel] = restore
                    ? (video->pages[1][pixel]
                       & (offset + byte >= 0x10000      ? 4
                              : offset + byte >= 0x8000 ? 6
                                                        : 15))
                    : 15;
            }
        }
    }
}

RESULT PelletAssets_Load(
    READER *const reader, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const allocator, PELLET_ASSETS **const out)
{
    *out = nullptr;
    FAIL_IF(
        Reader_Size(reader) != PELLET_EXECUTABLE_SIZE,
        "Unsupported REIIDEN.EXE length");
    FAIL_IF(
        scratch == nullptr || scratch_size < PELLET_EXECUTABLE_SIZE,
        "Missing executable scratch storage");
    Reader_Seek(reader, 0);
    Reader_ReadBytes(reader, scratch, PELLET_EXECUTABLE_SIZE);
    MUST(Reader_Result(reader));
    char digest[SHA256_HEX_SIZE];
    Sha256_ComputeHex(scratch, PELLET_EXECUTABLE_SIZE, digest);
    FAIL_IF(
        strcmp(
            digest,
            "76a5d241dead706e768bd1516d6aa539b589445e5941877073e8cb9e9ae8f62a")
            != 0,
        "Unsupported REIIDEN.EXE version");
    READER source;
    Reader_OpenBuffer(&source, scratch, PELLET_EXECUTABLE_SIZE, nullptr);
    FAIL_IF(Reader_ReadU16(&source) != 0x5a4d, "Invalid MZ signature");
    Reader_Seek(&source, 8);
    FAIL_IF(Reader_ReadU16(&source) != 896, "Unsupported MZ header");
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
    VIDEO *const video, const PELLET_ASSETS *const assets, const int16_t left,
    const int16_t top, const int16_t image)
{
    uint16_t offset = (uint16_t)(((uint16_t)left >> 3) + top * 80);
    for (int32_t row = 0; row < 8; row++) {
        M_Row(
            video, assets->pellets[image][(uint16_t)left & 7][row], offset,
            false);
        offset = (uint16_t)(offset + 80);
        if (Math_Wrap16(offset) >= 32000) {
            return;
        }
    }
}

void PelletAssets_Cloud(
    VIDEO *const video, const PELLET_ASSETS *const assets, const int16_t left,
    const int16_t top, const int16_t image, const bool restore)
{
    if (restore) {
        Video_SelectDrawingPage(video, 0);
    }
    uint16_t offset = (uint16_t)(left / 8 - (left % 8 < 0) + top * 80);
    for (int32_t row = 0; row < 16; row++) {
        M_Row(video, assets->clouds[image][row], offset, restore);
        offset = (uint16_t)(offset + 80);
    }
}
