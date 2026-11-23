#include "pc98/zun/bfnt.h"

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
    const uint16_t width = Reader_ReadU16(reader);
    const uint16_t height = Reader_ReadU16(reader);
    const uint16_t first = Reader_ReadU16(reader);
    const uint16_t last = Reader_ReadU16(reader);
    Reader_Skip(reader, 12);
    const uint16_t extension = Reader_ReadU16(reader);
    Reader_Skip(reader, 2);
    MUST(Reader_Result(reader));
    FAIL_IF(memcmp(signature, "BFNT\x1a", 5) != 0, "Invalid BFNT signature");
    FAIL_IF(
        (color != 0 && color != 3 && color != 0x83) || extension != 0,
        "Unsupported BFNT color mode or extension");
    FAIL_IF(
        width == 0 || width > 2040 || width % 8 != 0 || height == 0
            || height > 255 || first > last || last - first >= 512,
        "Invalid BFNT dimensions or range");
    BFNT sprites = {
        .width = width,
        .height = height,
        .count = last - first + 1,
        .first = first,
        .bits_per_pixel = color == 0 ? 1 : 4,
        .has_palette = color == 0x83,
    };
    const size_t pixel_count = (size_t)width * height * (size_t)sprites.count;
    const size_t packed_size =
        pixel_count / (sprites.bits_per_pixel == 1 ? 8 : 2);
    const size_t palette_size = sprites.has_palette ? 48 : 0;
    FAIL_IF(
        Reader_Remaining(reader) < palette_size + packed_size,
        "Truncated BFNT palette or pixels");
    if (sprites.has_palette) {
        for (size_t i = 0; i < 16; i++) {
            const uint8_t blue = Reader_ReadU8(reader);
            const uint8_t red = Reader_ReadU8(reader);
            const uint8_t green = Reader_ReadU8(reader);
            sprites.palette[i] =
                (GFX_COLOR) { red, green, blue, i == 0 ? 0 : 255 };
        }
    }
    uint8_t *const packed =
        Reader_ReadAlloc(reader, packed_size, &g_HeapAllocator);
    const RESULT result = Reader_Result(reader);
    if (!IS_OK(result)) {
        Mem_Free(packed);
        return result;
    }
    uint8_t *const pixels = Allocator_Alloc(allocator, pixel_count);
    if (sprites.bits_per_pixel == 1) {
        for (size_t i = 0; i < pixel_count; i++) {
            pixels[i] = (packed[i / 8] >> (7 - i % 8)) & 1;
        }
    } else {
        for (size_t i = 0; i < packed_size; i++) {
            pixels[i * 2] = packed[i] >> 4;
            pixels[i * 2 + 1] = packed[i] & 15;
        }
    }
    Mem_Free(packed);
    sprites.pixels = pixels;
    *out = sprites;
    return OK;
}
