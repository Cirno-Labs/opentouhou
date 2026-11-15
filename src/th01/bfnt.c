#include "th01/bfnt.h"

#include "core/base/memory.h"

#include <string.h>

RESULT Bfnt_Load(
    READER *const reader, const ALLOCATOR *const allocator, BFNT *const out)
{
    *out = (BFNT) {};
    uint8_t signature[5];
    Reader_ReadBytes(reader, signature, sizeof(signature));
    const uint8_t color = Reader_ReadU8(reader);
    Reader_Skip(reader, 2);
    const int32_t width = Reader_ReadU16(reader);
    const int32_t height = Reader_ReadU16(reader);
    const int32_t first = Reader_ReadU16(reader);
    const int32_t last = Reader_ReadU16(reader);
    Reader_Skip(reader, 12);
    const uint16_t extension_size = Reader_ReadU16(reader);
    Reader_Skip(reader, 2);
    MUST(Reader_Result(reader));
    FAIL_IF(memcmp(signature, "BFNT\x1a", 5) != 0, "Invalid BFNT signature");
    FAIL_IF(
        color != 0x83 || extension_size != 0,
        "Unsupported BFNT color mode or extension");
    FAIL_IF(
        width <= 0 || width > 640 || width % 8 != 0 || height <= 0
            || height > 400 || first != 0 || last >= 256,
        "Invalid BFNT sprite dimensions or range");
    const int32_t count = last + 1;
    const size_t size = (size_t)width * (size_t)height * (size_t)count;
    FAIL_IF(
        Reader_Remaining(reader) < 48
            || size / 2 > Reader_Remaining(reader) - 48,
        "Truncated BFNT sprites");
    BFNT sprites = { .width = width, .height = height, .count = count };
    for (int32_t i = 0; i < 16; i++) {
        const uint8_t blue = Reader_ReadU8(reader);
        const uint8_t red = Reader_ReadU8(reader);
        const uint8_t green = Reader_ReadU8(reader);
        sprites.palette[i] = (GFX_COLOR) { red, green, blue, i == 0 ? 0 : 255 };
    }
    sprites.pixels = Mem_Alloc(size);
    for (size_t i = 0; i < size; i += 2) {
        const uint8_t packed = Reader_ReadU8(reader);
        sprites.pixels[i] = packed >> 4;
        sprites.pixels[i + 1] = packed & 15;
    }
    const RESULT result = Reader_Result(reader);
    if (!IS_OK(result)) {
        Mem_Free(sprites.pixels);
        return result;
    }
    uint8_t *const pixels = Allocator_Alloc(allocator, size);
    memcpy(pixels, sprites.pixels, size);
    Mem_Free(sprites.pixels);
    sprites.pixels = pixels;
    *out = sprites;
    return OK;
}
