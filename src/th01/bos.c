#include "th01/bos.h"

#include "core/base/arena.h"

#include <string.h>

RESULT Bos_Load(
    READER *const reader, const ALLOCATOR *const allocator, BOS *const out)
{
    *out = (BOS) {};
    uint8_t magic[4];
    Reader_ReadBytes(reader, magic, sizeof(magic));
    const int32_t width_bytes = Reader_ReadU8(reader);
    Reader_Skip(reader, 1);
    const int32_t height = Reader_ReadU8(reader);
    Reader_Skip(reader, 1);
    const int32_t count = Reader_ReadU8(reader);
    Reader_Skip(reader, 7);
    Reader_Skip(reader, 48);
    MUST(Reader_Result(reader));
    FAIL_IF(memcmp(magic, "BOSS", 4) != 0, "Invalid BOS signature");
    FAIL_IF(width_bytes == 0 || height == 0, "Invalid BOS dimensions");
    FAIL_IF(count == 0, "Invalid BOS image count");
    const size_t plane_size = (size_t)width_bytes * (size_t)height;
    const size_t packed_size = (size_t)count * plane_size * 5;
    FAIL_IF(packed_size > Reader_Remaining(reader), "Truncated BOS sprites");
    ARENA staging;
    Arena_Init(&staging, "BOS staging", packed_size);
    const uint8_t *const packed =
        Reader_ReadAlloc(reader, packed_size, Arena_Allocator(&staging));
    RESULT result = Reader_Result(reader);
    if (!IS_OK(result)) {
        Arena_Free(&staging);
        return result;
    }
    const size_t pixel_count = (size_t)count * plane_size * 8;
    uint8_t *const pixels = Allocator_Alloc(allocator, pixel_count * 2);
    uint8_t *const coverage = pixels + pixel_count;
    for (int32_t image = 0; image < count; image++) {
        const uint8_t *const planes = packed + (size_t)image * plane_size * 5;
        for (size_t byte = 0; byte < plane_size; byte++) {
            for (int32_t bit = 0; bit < 8; bit++) {
                const size_t pixel =
                    ((size_t)image * plane_size + byte) * 8 + bit;
                const uint8_t mask = 0x80 >> bit;
                coverage[pixel] = (planes[byte] & mask) == 0;
                uint8_t color = 0;
                for (int32_t plane = 0; plane < 4; plane++) {
                    if ((planes[(size_t)(plane + 1) * plane_size + byte] & mask)
                        != 0) {
                        color |= 1 << plane;
                    }
                }
                pixels[pixel] = color;
            }
        }
    }
    Arena_Free(&staging);
    *out = (BOS) {
        .width = width_bytes * 8,
        .height = height,
        .count = count,
        .pixels = pixels,
        .coverage = coverage,
    };
    return OK;
}
