#include "th01/grc.h"

#include <string.h>

RESULT Grc_Load(
    READER *const reader, const ALLOCATOR *const allocator, GRC *const out)
{
    *out = (GRC) {};
    uint8_t magic[4];
    Reader_ReadBytes(reader, magic, sizeof(magic));
    const int32_t width_bytes = Reader_ReadI16(reader);
    const int32_t height = Reader_ReadI16(reader);
    const int32_t count = Reader_ReadU8(reader);
    Reader_Skip(reader, 7);
    Reader_Skip(reader, 48);
    MUST(Reader_Result(reader));
    FAIL_IF(memcmp(magic, "GRCG", 4) != 0, "Invalid GRC signature");
    FAIL_IF(width_bytes < 1 || height < 1, "Invalid GRC dimensions");
    FAIL_IF(count < 1 || count > GRC_MAX_IMAGES, "Invalid GRC image count");
    const size_t packed_size = (size_t)width_bytes * (size_t)height;
    FAIL_IF(packed_size > UINT16_MAX, "GRC image exceeds 16-bit byte count");
    FAIL_IF(
        (size_t)count * packed_size > Reader_Remaining(reader),
        "Truncated GRC sprites");
    const size_t size = (size_t)count * packed_size;
    uint8_t *const packed = Reader_ReadAlloc(reader, size, &g_HeapAllocator);
    MUST(Reader_Result(reader));
    GRC sprites = {
        .width = width_bytes * 8,
        .height = height,
        .count = count,
        .coverage = Allocator_Alloc(allocator, size * 8),
    };
    for (size_t i = 0; i < size; i++) {
        const uint8_t bits = packed[i];
        for (int32_t bit = 0; bit < 8; bit++) {
            sprites.coverage[i * 8 + bit] = (bits >> (7 - bit)) & 1;
        }
    }
    Mem_Free(packed);
    *out = sprites;
    return OK;
}
