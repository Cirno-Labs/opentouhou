#include "th02/mpn.h"

#include <string.h>

RESULT Mpn_Load(
    READER *const reader, const ALLOCATOR *const allocator, MPN *const out)
{
    *out = (MPN) {};
    uint8_t signature[4];
    Reader_ReadBytes(reader, signature, sizeof(signature));
    const uint16_t count = (uint16_t)Reader_ReadU8(reader) + 1;
    Reader_Skip(reader, 1);
    MUST(Reader_Result(reader));
    FAIL_IF(memcmp(signature, "MPTN", 4) != 0, "Invalid MPN signature");

    const size_t planar_size = (size_t)count * 128;
    FAIL_IF(
        Reader_Remaining(reader) < 48 + planar_size,
        "Truncated MPN palette or tiles");
    MPN tiles = { .count = count };
    for (size_t i = 0; i < 16; i++) {
        const uint8_t red = Reader_ReadU8(reader);
        const uint8_t green = Reader_ReadU8(reader);
        const uint8_t blue = Reader_ReadU8(reader);
        tiles.palette[i] = (GFX_COLOR) { red, green, blue, 255 };
    }
    uint8_t *const planes =
        Reader_ReadAlloc(reader, planar_size, &g_HeapAllocator);
    const RESULT result = Reader_Result(reader);
    if (!IS_OK(result)) {
        Mem_Free(planes);
        return result;
    }

    tiles.pixels = Allocator_Alloc(allocator, (size_t)count * 256);
    for (size_t tile = 0; tile < count; tile++) {
        for (size_t y = 0; y < MPN_TILE_HEIGHT; y++) {
            for (size_t x = 0; x < MPN_TILE_WIDTH; x++) {
                uint8_t index = 0;
                const size_t byte = tile * 128 + y * 2 + x / 8;
                for (uint8_t plane = 0; plane < 4; plane++) {
                    index |= ((planes[byte + plane * 32] >> (7 - x % 8)) & 1)
                        << plane;
                }
                tiles.pixels[tile * 256 + y * 16 + x] = index;
            }
        }
    }
    Mem_Free(planes);
    *out = tiles;
    return OK;
}
