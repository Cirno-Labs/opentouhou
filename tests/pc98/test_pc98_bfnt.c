#include "tests/test.h"

#include "core/base/arena.h"
#include "core/io/reader.h"
#include "pc98/zun/bfnt.h"

#include <string.h>

static void M_Header(uint8_t *const bytes, const uint8_t color)
{
    memset(bytes, 0, 32);
    memcpy(bytes, "BFNT\x1a", 5);
    bytes[5] = color;
    bytes[6] = 0x16;
    bytes[8] = 8;
    bytes[10] = 2;
    bytes[14] = 1;
}

static RESULT M_Load(
    const void *const bytes, const size_t size, ARENA *const arena,
    BFNT *const sprites)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    const RESULT result = Bfnt_Load(&reader, Arena_Allocator(arena), sprites);
    Reader_Close(&reader);
    return result;
}

TEST(color_pixels_and_embedded_palette_keep_the_original_component_order)
{
    uint8_t bytes[96] = {};
    M_Header(bytes, 0x83);
    bytes[35] = 0x20;
    bytes[36] = 0x70;
    bytes[37] = 0xf0;
    bytes[80] = 0x1f;
    bytes[88] = 0x23;
    ARENA arena;
    Arena_Init(&arena, "BFNT test", 8192);
    BFNT sprites;
    const bool loaded = Result_Log(
        M_Load(bytes, sizeof(bytes), &arena, &sprites), LOG_LEVEL_ERROR);
    CHECK(loaded);
    if (loaded) {
        CHECK_EQ_INT(sprites.width, 8);
        CHECK_EQ_INT(sprites.height, 2);
        CHECK_EQ_INT(sprites.count, 2);
        CHECK_EQ_INT(sprites.first, 0);
        CHECK_EQ_INT(sprites.bits_per_pixel, 4);
        CHECK(sprites.has_palette);
        CHECK_EQ_INT(sprites.pixels[0], 1);
        CHECK_EQ_INT(sprites.pixels[1], 15);
        CHECK_EQ_INT(sprites.pixels[16], 2);
        CHECK_EQ_INT(sprites.pixels[17], 3);
        CHECK_EQ_INT(sprites.palette[1].blue, 0x20);
        CHECK_EQ_INT(sprites.palette[1].red, 0x70);
        CHECK_EQ_INT(sprites.palette[1].green, 0xf0);
        CHECK_EQ_INT(sprites.palette[0].alpha, 0);
        CHECK_EQ_INT(sprites.palette[1].alpha, 255);
    }
    memset(bytes, 0, sizeof(bytes));
    if (loaded) {
        CHECK_EQ_INT(sprites.pixels[17], 3);
    }
    Arena_Free(&arena);
}

TEST(color_files_without_a_palette_keep_the_first_character_code)
{
    uint8_t bytes[37] = {};
    M_Header(bytes, 3);
    bytes[10] = 1;
    bytes[12] = bytes[14] = 10;
    memcpy(bytes + 32, "\x12\x34\x56\x70\x5a", 5);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    ARENA arena;
    Arena_Init(&arena, "BFNT test", 8192);
    BFNT sprites;
    const bool loaded = Result_Log(
        Bfnt_Load(&reader, Arena_Allocator(&arena), &sprites), LOG_LEVEL_ERROR);
    CHECK(loaded);
    if (loaded) {
        CHECK(!sprites.has_palette);
        CHECK_EQ_INT(sprites.bits_per_pixel, 4);
        CHECK_EQ_INT(sprites.first, 10);
        CHECK_EQ_INT(sprites.count, 1);
        for (size_t i = 0; i < 7; i++) {
            CHECK_EQ_INT(sprites.pixels[i], i + 1);
        }
        CHECK_EQ_INT(sprites.pixels[7], 0);
        CHECK_EQ_INT(Reader_ReadU8(&reader), 0x5a);
    }
    Reader_Close(&reader);
    Arena_Free(&arena);
}

TEST(monochrome_masks_read_most_significant_bits_first)
{
    uint8_t bytes[36] = {};
    M_Header(bytes, 0);
    bytes[32] = 0x81;
    bytes[33] = 0x3c;
    bytes[34] = 0xff;
    ARENA arena;
    Arena_Init(&arena, "BFNT test", 8192);
    BFNT sprites;
    const bool loaded = Result_Log(
        M_Load(bytes, sizeof(bytes), &arena, &sprites), LOG_LEVEL_ERROR);
    CHECK(loaded);
    if (loaded) {
        CHECK_EQ_INT(sprites.bits_per_pixel, 1);
        CHECK(!sprites.has_palette);
        const uint8_t expected[] = { 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
                                     1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1,
                                     1, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        CHECK(memcmp(sprites.pixels, expected, sizeof(expected)) == 0);
    }
    Arena_Free(&arena);
}

TEST(rejects_every_truncated_prefix_in_all_three_modes)
{
    const uint8_t modes[] = { 0, 3, 0x83 };
    const size_t sizes[] = { 36, 48, 96 };
    ARENA arena;
    Arena_Init(&arena, "BFNT test", 8192);
    for (size_t mode = 0; mode < 3; mode++) {
        uint8_t bytes[96] = {};
        M_Header(bytes, modes[mode]);
        for (size_t size = 0; size < sizes[mode]; size++) {
            BFNT sprites;
            CHECK(!Result_Ignore(M_Load(bytes, size, &arena, &sprites)));
            CHECK(sprites.pixels == nullptr);
            CHECK_EQ_INT(sprites.count, 0);
            CHECK_EQ_INT(arena.used, 0);
        }
    }
    Arena_Free(&arena);
}

TEST(rejects_invalid_headers_without_asset_allocations)
{
    const size_t offsets[] = { 0, 5, 5, 5, 8, 8, 9, 10, 11, 12, 15, 28 };
    const uint8_t values[] = { 'X', 1, 0x80, 0xff, 0, 7, 8, 0, 1, 2, 2, 1 };
    ARENA arena;
    Arena_Init(&arena, "BFNT test", 8192);
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        uint8_t bytes[96] = {};
        M_Header(bytes, 0x83);
        bytes[offsets[i]] = values[i];
        BFNT sprites;
        CHECK(!Result_Ignore(M_Load(bytes, sizeof(bytes), &arena, &sprites)));
        CHECK(sprites.pixels == nullptr);
        CHECK_EQ_INT(arena.used, 0);
    }
    Arena_Free(&arena);
}

TEST(supports_the_full_512_pattern_capacity)
{
    uint8_t bytes[32 + 512 * 4];
    M_Header(bytes, 3);
    bytes[10] = 1;
    bytes[14] = 0xff;
    bytes[15] = 1;
    memset(bytes + 32, 0xa5, sizeof(bytes) - 32);
    ARENA arena;
    Arena_Init(&arena, "BFNT test", 8192);
    BFNT sprites;
    const bool loaded = Result_Log(
        M_Load(bytes, sizeof(bytes), &arena, &sprites), LOG_LEVEL_ERROR);
    CHECK(loaded);
    if (loaded) {
        CHECK_EQ_INT(sprites.count, 512);
        CHECK_EQ_INT(sprites.pixels[4094], 10);
        CHECK_EQ_INT(sprites.pixels[4095], 5);
    }
    Arena_Free(&arena);
}

TEST(assets_share_an_arena_and_survive_reader_close)
{
    uint8_t bytes[96] = {};
    M_Header(bytes, 0x83);
    bytes[80] = 0x1f;
    bytes[88] = 0x23;
    ARENA arena;
    Arena_Init(&arena, "BFNT assets", 1024);
    BFNT first;
    BFNT second;
    CHECK(Result_Log(
        M_Load(bytes, sizeof(bytes), &arena, &first), LOG_LEVEL_ERROR));
    CHECK(Result_Log(
        M_Load(bytes, sizeof(bytes), &arena, &second), LOG_LEVEL_ERROR));
    memset(bytes, 0, sizeof(bytes));
    CHECK(first.pixels != second.pixels);
    CHECK_EQ_INT(first.pixels[1], 15);
    CHECK_EQ_INT(second.pixels[17], 3);
    CHECK_EQ_INT(arena.used, 64);
    Arena_Reset(&arena);
    CHECK_EQ_INT(arena.used, 0);
    Arena_Free(&arena);
}
