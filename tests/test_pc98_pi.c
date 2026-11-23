#include "pc98/zun/pi.h"

#include "core/base/arena.h"

#include "test.h"

typedef struct {
    uint8_t bytes[4096];
    size_t size, bit_count;
} PI_BUILDER;

static void M_Byte(PI_BUILDER *const file, const uint8_t value)
{
    file->bytes[file->size++] = value;
}

static void M_Word(PI_BUILDER *const file, const uint16_t value)
{
    M_Byte(file, value >> 8);
    M_Byte(file, value);
}

static void M_Bits(PI_BUILDER *const file, const char *text)
{
    while (*text != '\0') {
        if (file->bit_count % 8 == 0) {
            M_Byte(file, 0);
        }
        if (*text++ == '1') {
            file->bytes[file->size - 1] |= 0x80 >> (file->bit_count % 8);
        }
        file->bit_count++;
    }
}

static void M_Length(PI_BUILDER *const file, const uint32_t pairs)
{
    int32_t exponent = 0;
    for (uint32_t rest = pairs; rest > 1; rest >>= 1) {
        M_Bits(file, "1");
        exponent++;
    }
    M_Bits(file, "0");
    for (int32_t bit = exponent - 1; bit >= 0; bit--) {
        M_Bits(file, (pairs & (1u << bit)) != 0 ? "1" : "0");
    }
}

static PI_BUILDER M_Header(
    const bool zn, const int32_t width, const int32_t height,
    const bool palette, const uint8_t *const extension,
    const size_t extension_size)
{
    PI_BUILDER file = {};
    M_Byte(&file, zn ? 'Z' : 'P');
    M_Byte(&file, zn ? 'N' : 'i');
    M_Byte(&file, 0x1a);
    M_Byte(&file, 0);
    M_Byte(&file, palette ? 0 : 0x80);
    M_Word(&file, 0x0102);
    M_Byte(&file, 4);
    M_Byte(&file, 'P');
    M_Byte(&file, 'C');
    M_Byte(&file, '9');
    M_Byte(&file, '8');
    M_Word(&file, extension_size);
    for (size_t i = 0; i < extension_size; i++) {
        M_Byte(&file, extension[i]);
    }
    M_Word(&file, width);
    M_Word(&file, height);
    if (palette) {
        for (int32_t i = 0; i < 16; i++) {
            M_Byte(&file, zn ? i : i * 16 + 3);
            M_Byte(&file, zn ? 15 - i : 255 - i * 16);
            M_Byte(&file, zn ? i / 2 : i * 7);
        }
    }
    return file;
}

static void M_Alternating(PI_BUILDER *const file, const uint32_t pixels)
{
    M_Bits(file, "011111001");
    M_Bits(file, "00");
    M_Length(file, (pixels + 1) / 2);
}

static RESULT M_Load(
    const PI_BUILDER *const file, const ALLOCATOR *const allocator,
    PI_IMAGE *const image)
{
    READER reader;
    Reader_OpenBuffer(&reader, file->bytes, file->size, nullptr);
    return Pi_Load(&reader, allocator, image);
}

static void M_CheckPixels(
    const PI_IMAGE *const image, const uint8_t *const pixels,
    const size_t count)
{
    CHECK(image->pixels != nullptr);
    if (image->pixels != nullptr) {
        for (size_t i = 0; i < count; i++) {
            CHECK_EQ_INT(image->pixels[i], pixels[i]);
        }
    }
}

TEST(loads_nibble_and_byte_rgb_palettes_separately)
{
    ARENA arena;
    Arena_Init(&arena, "Pi test", 1024 * 1024);
    for (int32_t zn = 0; zn < 2; zn++) {
        PI_BUILDER file = M_Header(zn != 0, 8, 1, true, nullptr, 0);
        M_Alternating(&file, 8);
        PI_IMAGE image;
        CHECK(Result_Log(
            M_Load(&file, Arena_Allocator(&arena), &image), LOG_LEVEL_ERROR));
        CHECK_EQ_INT(image.width, 8);
        CHECK_EQ_INT(image.height, 1);
        CHECK(image.has_palette);
        CHECK_EQ_INT(image.palette[4].red, zn != 0 ? 68 : 67);
        CHECK_EQ_INT(image.palette[3].green, zn != 0 ? 204 : 207);
        CHECK_EQ_INT(image.palette[3].blue, zn != 0 ? 17 : 21);
        CHECK_EQ_INT(image.palette[0].alpha, 255);
        CHECK_EQ_INT(image.transparent, -1);
        const uint8_t expected[] = { 1, 14, 1, 14, 1, 14, 1, 14 };
        M_CheckPixels(&image, expected, sizeof(expected));
    }
    Arena_Free(&arena);
}

TEST(loads_the_complete_default_palette)
{
    ARENA arena;
    Arena_Init(&arena, "Pi test", 1024 * 1024);
    PI_BUILDER file = M_Header(true, 8, 1, false, nullptr, 0);
    M_Alternating(&file, 8);
    PI_IMAGE image;
    CHECK(Result_Log(
        M_Load(&file, Arena_Allocator(&arena), &image), LOG_LEVEL_ERROR));
    CHECK(!image.has_palette);
    CHECK_EQ_INT(image.palette[1].blue, 119);
    CHECK_EQ_INT(image.palette[9].blue, 255);
    CHECK_EQ_INT(image.palette[15].red, 255);
    CHECK_EQ_INT(image.palette[15].green, 255);
    CHECK_EQ_INT(image.palette[15].blue, 255);
    Arena_Free(&arena);
}

TEST(decodes_every_move_to_front_rank)
{
    ARENA arena;
    Arena_Init(&arena, "Pi test", 1024 * 1024);
    const char *const ranks[] = {
        "10",     "11",     "000",    "001",    "01000",  "01001",
        "01010",  "01011",  "011000", "011001", "011010", "011011",
        "011100", "011101", "011110", "011111",
    };
    for (int32_t rank = 0; rank < 16; rank++) {
        PI_BUILDER file = M_Header(true, 2, 1, true, nullptr, 0);
        M_Bits(&file, ranks[rank]);
        M_Bits(&file, "10000");
        PI_IMAGE image;
        CHECK(Result_Log(
            M_Load(&file, Arena_Allocator(&arena), &image), LOG_LEVEL_ERROR));
        const uint8_t color = (16 - rank) & 15;
        const uint8_t expected[] = { color, color };
        M_CheckPixels(&image, expected, sizeof(expected));
    }
    Arena_Free(&arena);
}

TEST(keeps_images_independent_of_readers_and_other_loads)
{
    ARENA arena;
    Arena_Init(&arena, "Pi test", 1024 * 1024);
    PI_BUILDER file = M_Header(true, 8, 1, true, nullptr, 0);
    M_Alternating(&file, 8);
    READER reader;
    Reader_OpenOwnedBuffer(
        &reader, Mem_Dup(file.bytes, file.size), file.size, &g_HeapAllocator,
        nullptr);
    PI_IMAGE first;
    CHECK(Result_Log(
        Pi_Load(&reader, Arena_Allocator(&arena), &first), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Reader_Size(&reader), file.size);
    Reader_Close(&reader);
    PI_IMAGE second;
    CHECK(Result_Log(
        M_Load(&file, Arena_Allocator(&arena), &second), LOG_LEVEL_ERROR));
    const uint8_t expected[] = { 1, 14, 1, 14, 1, 14, 1, 14 };
    CHECK(first.pixels != second.pixels);
    M_CheckPixels(&first, expected, sizeof(expected));
    M_CheckPixels(&second, expected, sizeof(expected));
    Arena_Free(&arena);
}

TEST(exposes_embedded_position_and_first_transparency_record)
{
    ARENA arena;
    Arena_Init(&arena, "Pi test", 1024 * 1024);
    const uint8_t extension[] = {
        1, 1, 44, 1, 255, 2, 0, 0, 0, 14, 2, 0, 0, 0, 3,
    };
    PI_BUILDER file = M_Header(true, 8, 1, true, extension, sizeof(extension));
    M_Alternating(&file, 8);
    PI_IMAGE image;
    CHECK(Result_Log(
        M_Load(&file, Arena_Allocator(&arena), &image), LOG_LEVEL_ERROR));
    CHECK(image.has_position);
    CHECK_EQ_INT(image.x, 300);
    CHECK_EQ_INT(image.y, 511);
    CHECK_EQ_INT(image.transparent, 14);
    Arena_Free(&arena);
}

TEST(skips_unknown_extension_records_to_the_image)
{
    ARENA arena;
    Arena_Init(&arena, "Pi test", 1024 * 1024);
    const uint8_t extension[] = { 99, 1, 255, 255, 255, 255 };
    PI_BUILDER file = M_Header(true, 8, 1, true, extension, sizeof(extension));
    M_Alternating(&file, 8);
    PI_IMAGE image;
    CHECK(Result_Log(
        M_Load(&file, Arena_Allocator(&arena), &image), LOG_LEVEL_ERROR));
    CHECK(!image.has_position);
    CHECK_EQ_INT(image.x, 0);
    CHECK_EQ_INT(image.y, 0);
    Arena_Free(&arena);
}

TEST(decodes_each_backreference_and_context_literals)
{
    ARENA arena;
    Arena_Init(&arena, "Pi test", 1024 * 1024);
    const char *const positions[] = { "00", "01", "10", "110", "111" };
    for (int32_t mode = 0; mode < 5; mode++) {
        PI_BUILDER file = M_Header(true, 8, 1, true, nullptr, 0);
        M_Bits(&file, "011111001");
        M_Bits(&file, positions[mode]);
        M_Length(&file, 2);
        M_Bits(&file, positions[mode]);
        M_Bits(&file, "101111011");
        PI_IMAGE image;
        CHECK(Result_Log(
            M_Load(&file, Arena_Allocator(&arena), &image), LOG_LEVEL_ERROR));
        const uint8_t a = mode >= 3 ? 14 : 1;
        const uint8_t b = mode >= 3 ? 1 : 14;
        const uint8_t expected[] = { a, b, a, b, 14, 13, 13, 12 };
        M_CheckPixels(&image, expected, sizeof(expected));
    }
    Arena_Free(&arena);
}

TEST(compares_actual_distances_when_position_codes_alias)
{
    ARENA arena;
    Arena_Init(&arena, "Pi test", 1024 * 1024);
    PI_BUILDER file = M_Header(true, 4, 2, true, nullptr, 0);
    M_Bits(&file, "011111001");
    M_Bits(&file, "000");
    M_Bits(&file, "0110110");
    M_Bits(&file, "10100");
    PI_IMAGE image;
    CHECK(Result_Log(
        M_Load(&file, Arena_Allocator(&arena), &image), LOG_LEVEL_ERROR));
    const uint8_t expected[] = { 1, 14, 14, 13, 1, 14, 1, 14 };
    M_CheckPixels(&image, expected, sizeof(expected));
    Arena_Free(&arena);
}

TEST(keeps_the_horizontal_predictor_identity_for_solid_pairs)
{
    ARENA arena;
    Arena_Init(&arena, "Pi test", 1024 * 1024);
    PI_BUILDER file = M_Header(true, 2, 2, true, nullptr, 0);
    M_Bits(&file, "01111110");
    M_Bits(&file, "000010");
    PI_IMAGE image;
    CHECK(Result_Log(
        M_Load(&file, Arena_Allocator(&arena), &image), LOG_LEVEL_ERROR));
    const uint8_t expected[] = { 1, 1, 1, 1 };
    M_CheckPixels(&image, expected, sizeof(expected));
    Arena_Free(&arena);
}

TEST(decodes_odd_width_and_partial_four_line_height)
{
    ARENA arena;
    Arena_Init(&arena, "Pi test", 1024 * 1024);
    PI_BUILDER file = M_Header(true, 3, 5, true, nullptr, 0);
    M_Alternating(&file, 15);
    PI_IMAGE image;
    CHECK(Result_Log(
        M_Load(&file, Arena_Allocator(&arena), &image), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(image.width, 3);
    CHECK_EQ_INT(image.height, 5);
    if (image.pixels != nullptr) {
        for (int32_t i = 0; i < 15; i++) {
            CHECK_EQ_INT(image.pixels[i], i % 2 == 0 ? 1 : 14);
        }
    }
    Arena_Free(&arena);
}

TEST(bounds_long_overlapping_runs_to_the_image)
{
    ARENA arena;
    Arena_Init(&arena, "Pi test", 1024 * 1024);
    PI_BUILDER file = M_Header(true, 2, 1, true, nullptr, 0);
    M_Alternating(&file, 2000000);
    PI_IMAGE image;
    CHECK(Result_Log(
        M_Load(&file, Arena_Allocator(&arena), &image), LOG_LEVEL_ERROR));
    const uint8_t expected[] = { 1, 14 };
    M_CheckPixels(&image, expected, sizeof(expected));
    CHECK_EQ_INT(arena.used, sizeof(expected));
    Arena_Reset(&arena);
    CHECK_EQ_INT(arena.used, 0);
    Arena_Free(&arena);
}

TEST(rejects_every_truncated_prefix)
{
    ARENA arena;
    Arena_Init(&arena, "Pi test", 1024 * 1024);
    const uint8_t extension[] = { 1, 0, 3, 0, 4 };
    PI_BUILDER file = M_Header(true, 8, 3, true, extension, sizeof(extension));
    M_Alternating(&file, 24);
    for (size_t size = 0; size < file.size; size++) {
        READER reader;
        Reader_OpenBuffer(&reader, file.bytes, size, nullptr);
        PI_IMAGE image;
        CHECK(
            !Result_Ignore(Pi_Load(&reader, Arena_Allocator(&arena), &image)));
        CHECK(image.pixels == nullptr);
        CHECK_EQ_INT(arena.used, 0);
        CHECK_EQ_INT(image.width, 0);
    }
    Arena_Free(&arena);
}

TEST(rejects_invalid_signatures_dimensions_planes_and_palette)
{
    ARENA arena;
    Arena_Init(&arena, "Pi test", 1024 * 1024);
    const int32_t offsets[] = { 0, 7, 14, 15, 16, 17, 18 };
    const uint8_t values[] = { 'X', 8, 5, 1, 4, 0, 16 };
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        PI_BUILDER file = M_Header(true, 8, 1, true, nullptr, 0);
        M_Alternating(&file, 8);
        file.bytes[offsets[i]] = values[i];
        PI_IMAGE image;
        CHECK(!Result_Ignore(M_Load(&file, Arena_Allocator(&arena), &image)));
        CHECK(image.pixels == nullptr);
        CHECK_EQ_INT(arena.used, 0);
    }
    const int32_t dimensions[][2] = {
        { 0, 1 }, { 1, 1 }, { 1281, 1 }, { 8, 0 }, { 8, 801 },
    };
    for (size_t i = 0; i < sizeof(dimensions) / sizeof(dimensions[0]); i++) {
        PI_BUILDER file = M_Header(
            true, dimensions[i][0], dimensions[i][1], true, nullptr, 0);
        M_Alternating(&file, 8);
        PI_IMAGE image;
        CHECK(!Result_Ignore(M_Load(&file, Arena_Allocator(&arena), &image)));
        CHECK(image.pixels == nullptr);
        CHECK_EQ_INT(arena.used, 0);
    }
    Arena_Free(&arena);
}

TEST(rejects_incomplete_known_extensions)
{
    ARENA arena;
    Arena_Init(&arena, "Pi test", 1024 * 1024);
    const uint8_t extension[] = { 2, 0, 0 };
    PI_BUILDER file = M_Header(true, 8, 1, true, extension, sizeof(extension));
    M_Alternating(&file, 8);
    PI_IMAGE image;
    CHECK(!Result_Ignore(M_Load(&file, Arena_Allocator(&arena), &image)));
    CHECK(image.pixels == nullptr);
    Arena_Free(&arena);
}

TEST(rejects_unterminated_gamma_lengths)
{
    ARENA arena;
    Arena_Init(&arena, "Pi test", 1024 * 1024);
    PI_BUILDER file = M_Header(true, 8, 1, true, nullptr, 0);
    M_Bits(&file, "01111100100");
    M_Bits(&file, "1111111111111111111111111111111");
    PI_IMAGE image;
    CHECK(!Result_Ignore(M_Load(&file, Arena_Allocator(&arena), &image)));
    CHECK(image.pixels == nullptr);
    Arena_Free(&arena);
}
