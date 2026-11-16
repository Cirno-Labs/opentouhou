#include "th01/ptn.h"

#include <string.h>

RESULT Ptn_Load(
    READER *const reader, const ALLOCATOR *const allocator, PTN *const out)
{
    *out = (PTN) {};
    uint8_t magic[4];
    Reader_ReadBytes(reader, magic, sizeof(magic));
    Reader_ReadU8(reader);
    const int32_t count = Reader_ReadI8(reader);
    MUST(Reader_Result(reader));
    FAIL_IF(memcmp(magic, "HPTN", 4) != 0, "Invalid PTN signature");
    FAIL_IF(count < 1 || count > PTN_MAX_IMAGES, "Invalid PTN image count");
    FAIL_IF(
        Reader_Remaining(reader) < 48 + (size_t)count * 513,
        "Truncated PTN sprites");
    PTN sprites = { .count = count };
    for (int32_t i = 0; i < 16; i++) {
        const uint8_t red = Reader_ReadU8(reader);
        const uint8_t green = Reader_ReadU8(reader);
        const uint8_t blue = Reader_ReadU8(reader);
        FAIL_IF(
            red > 15 || green > 15 || blue > 15,
            "Invalid PTN palette component");
        sprites.palette[i] =
            (GFX_COLOR) { red * 17, green * 17, blue * 17, 255 };
    }
    MUST(Reader_Result(reader));
    sprites.pixels = Mem_AllocZ((size_t)count * PTN_WIDTH * PTN_HEIGHT);
    for (int32_t image = 0; image < count; image++) {
        Reader_ReadU8(reader);
        uint8_t *const pixels = sprites.pixels + image * PTN_WIDTH * PTN_HEIGHT;
        for (int32_t plane = 0; plane < 4; plane++) {
            for (int32_t byte = 0; byte < PTN_WIDTH * PTN_HEIGHT / 8; byte++) {
                const uint8_t bits = Reader_ReadU8(reader);
                for (int32_t bit = 0; bit < 8; bit++) {
                    pixels[byte * 8 + bit] |= ((bits >> (7 - bit)) & 1)
                        << plane;
                }
            }
        }
    }
    const RESULT result = Reader_Result(reader);
    if (!IS_OK(result)) {
        Mem_Free(sprites.pixels);
        return result;
    }
    const size_t size = (size_t)count * PTN_WIDTH * PTN_HEIGHT;
    uint8_t *const pixels = Allocator_Alloc(allocator, size);
    memcpy(pixels, sprites.pixels, size);
    Mem_Free(sprites.pixels);
    sprites.pixels = pixels;
    *out = sprites;
    return OK;
}
