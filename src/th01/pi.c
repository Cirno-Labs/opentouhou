#include "th01/pi.h"

#include <string.h>

typedef struct {
    READER *reader;
    uint8_t byte;
    int32_t bits_left;
    uint8_t colors[16][16];
} BIT_READER;

static const GFX_COLOR m_DefaultPalette[16] = {
    { 0, 0, 0, 255 },       { 0, 0, 119, 255 },     { 119, 0, 0, 255 },
    { 119, 0, 119, 255 },   { 0, 119, 0, 255 },     { 0, 119, 119, 255 },
    { 119, 119, 0, 255 },   { 119, 119, 119, 255 }, { 0, 0, 0, 255 },
    { 0, 0, 255, 255 },     { 255, 0, 0, 255 },     { 255, 0, 255, 255 },
    { 0, 255, 0, 255 },     { 0, 255, 255, 255 },   { 255, 255, 0, 255 },
    { 255, 255, 255, 255 },
};

static uint16_t M_ReadBigWord(READER *const reader)
{
    const uint16_t high = Reader_ReadU8(reader);
    return (high << 8) | Reader_ReadU8(reader);
}

static uint32_t M_ReadBits(BIT_READER *const bits, const int32_t count)
{
    uint32_t value = 0;
    for (int32_t i = 0; i < count; i++) {
        if (bits->bits_left == 0) {
            bits->byte = Reader_ReadU8(bits->reader);
            bits->bits_left = 8;
        }
        value = (value << 1) | ((bits->byte >> --bits->bits_left) & 1);
    }
    return value;
}

static uint8_t M_ReadColor(BIT_READER *const bits, const uint8_t context)
{
    uint32_t base = 0;
    int32_t count = 1;
    if (M_ReadBits(bits, 1) == 0) {
        base = 2;
        if (M_ReadBits(bits, 1) != 0) {
            base = 4;
            count = 2;
            if (M_ReadBits(bits, 1) != 0) {
                base = 8;
                count = 3;
            }
        }
    }
    const uint32_t index = base + M_ReadBits(bits, count);
    uint8_t *const row = bits->colors[context];
    const uint8_t color = row[index];
    memmove(row + 1, row, index);
    row[0] = color;
    return color;
}

static RESULT M_ReadExtension(READER *const reader, PI_IMAGE *const image)
{
    const size_t size = M_ReadBigWord(reader);
    READER extension = Reader_Slice(reader, size);
    MUST(Reader_Result(reader));
    while (Reader_Remaining(&extension) != 0) {
        const uint8_t type = Reader_ReadU8(&extension);
        if (type == 1) {
            image->x = M_ReadBigWord(&extension);
            image->y = M_ReadBigWord(&extension);
            image->has_position = true;
        } else if (type == 2) {
            Reader_Skip(&extension, 3);
            const uint8_t color = Reader_ReadU8(&extension);
            if (image->transparent < 0) {
                image->transparent = color & 15;
            }
        } else {
            break;
        }
        MUST(Reader_Result(&extension));
    }
    return OK;
}

static RESULT M_ReadHeader(READER *const reader, PI_IMAGE *const image)
{
    const uint8_t first = Reader_ReadU8(reader);
    const uint8_t second = Reader_ReadU8(reader);
    MUST(Reader_Result(reader));
    const bool zn = first == 'Z' && second == 'N';
    FAIL_IF(!zn && (first != 'P' || second != 'i'), "Invalid Pi signature");
    while (Reader_ReadU8(reader) != 0 && !Reader_Failed(reader)) {}
    const uint8_t mode = Reader_ReadU8(reader);
    Reader_Skip(reader, 2);
    const uint8_t planes = Reader_ReadU8(reader);
    Reader_Skip(reader, 4);
    MUST(Reader_Result(reader));
    FAIL_IF(planes != 4, "Unsupported Pi plane count");
    MUST(M_ReadExtension(reader, image));
    image->width = M_ReadBigWord(reader);
    image->height = M_ReadBigWord(reader);
    MUST(Reader_Result(reader));
    FAIL_IF(
        image->width < 2 || image->width > 1280 || image->height < 1
            || image->height > 800,
        "Unsupported Pi image dimensions");
    image->has_palette = (mode & 0x80) == 0;
    if (image->has_palette) {
        for (int32_t i = 0; i < 16; i++) {
            const uint8_t red = Reader_ReadU8(reader);
            const uint8_t green = Reader_ReadU8(reader);
            const uint8_t blue = Reader_ReadU8(reader);
            MUST(Reader_Result(reader));
            FAIL_IF(
                zn && (red > 15 || green > 15 || blue > 15),
                "Invalid ZN palette component");
            image->palette[i] = zn
                ? (GFX_COLOR) { red * 17, green * 17, blue * 17, 255 }
                : (GFX_COLOR) { red, green, blue, 255 };
        }
    } else {
        memcpy(image->palette, m_DefaultPalette, sizeof(image->palette));
    }
    return OK;
}

static RESULT M_Decode(READER *const reader, PI_IMAGE *const image)
{
    BIT_READER bits = { .reader = reader };
    for (int32_t context = 0; context < 16; context++) {
        for (int32_t index = 0; index < 16; index++) {
            bits.colors[context][index] = (context - index) & 15;
        }
    }
    const uint8_t first = M_ReadColor(&bits, 0);
    const uint8_t second = M_ReadColor(&bits, first);
    MUST(Reader_Result(reader));
    const size_t seed = (size_t)image->width * 2;
    const size_t end = seed + (size_t)image->width * (size_t)image->height;
    for (size_t i = 0; i < seed; i += 2) {
        image->pixels[i] = first;
        image->pixels[i + 1] = second;
    }
    size_t cursor = seed;
    size_t previous_prediction = 0;
    while (cursor < end) {
        uint32_t position = M_ReadBits(&bits, 2);
        if (position == 3) {
            position += M_ReadBits(&bits, 1);
        }
        size_t distance;
        switch (position) {
        case 0:
            distance =
                image->pixels[cursor - 1] == image->pixels[cursor - 2] ? 2 : 4;
            break;
        case 1:
            distance = image->width;
            break;
        case 2:
            distance = image->width * 2;
            break;
        case 3:
            distance = image->width - 1;
            break;
        default:
            distance = image->width + 1;
            break;
        }
        // The horizontal predictor keeps its four-byte identity even when
        // a solid pair shortens the actual copy distance to two bytes.
        const size_t prediction = position == 0 ? 4 : distance;
        if (prediction == previous_prediction) {
            do {
                for (int32_t i = 0; i < 2 && cursor < end; i++) {
                    image->pixels[cursor] =
                        M_ReadColor(&bits, image->pixels[cursor - 1]);
                    cursor++;
                }
                MUST(Reader_Result(reader));
            } while (cursor < end && M_ReadBits(&bits, 1) != 0);
            previous_prediction = 0;
        } else {
            int32_t exponent = 0;
            while (M_ReadBits(&bits, 1) != 0 && !Reader_Failed(reader)) {
                FAIL_IF(++exponent > 30, "Pi run length is too large");
            }
            const size_t pairs = (1u << exponent) | M_ReadBits(&bits, exponent);
            MUST(Reader_Result(reader));
            const size_t count =
                pairs * 2 < end - cursor ? pairs * 2 : end - cursor;
            for (size_t i = 0; i < count; i++) {
                image->pixels[cursor] = image->pixels[cursor - distance];
                cursor++;
            }
            previous_prediction = prediction;
        }
    }
    memmove(image->pixels, image->pixels + seed, end - seed);
    return OK;
}

RESULT Pi_Load(
    READER *const reader, const ALLOCATOR *const allocator, PI_IMAGE *const out)
{
    *out = (PI_IMAGE) {};
    PI_IMAGE image = { .transparent = -1 };
    MUST(M_ReadHeader(reader, &image));
    image.pixels = Mem_Alloc((size_t)image.width * ((size_t)image.height + 2));
    const RESULT result = M_Decode(reader, &image);
    if (!IS_OK(result)) {
        Mem_Free(image.pixels);
        return result;
    }
    const size_t size = (size_t)image.width * (size_t)image.height;
    uint8_t *const pixels = Allocator_Alloc(allocator, size);
    memcpy(pixels, image.pixels, size);
    Mem_Free(image.pixels);
    image.pixels = pixels;
    *out = image;
    return OK;
}
