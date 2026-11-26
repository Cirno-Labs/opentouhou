#include "th02/mpn.h"

#include "core/base/arena.h"

#include "test.h"

static void M_Header(uint8_t *const bytes, const uint16_t count)
{
    memcpy(bytes, "MPTN", 4);
    bytes[4] = (uint8_t)(count - 1);
    bytes[5] = 0xa5;
    for (size_t i = 0; i < 16; i++) {
        bytes[6 + i * 3] = (uint8_t)(i * 13);
        bytes[7 + i * 3] = (uint8_t)(i * 7);
        bytes[8 + i * 3] = (uint8_t)(i * 3);
    }
}

TEST(loads_planar_tiles_and_opaque_rgb_palette)
{
    uint8_t bytes[54 + 256] = {};
    M_Header(bytes, 2);
    bytes[54] = 0x80;
    bytes[54 + 32 + 1] = 0x80;
    bytes[54 + 64 + 30] = 0x01;
    bytes[54 + 96 + 31] = 0x01;
    memset(bytes + 54 + 128, 255, 128);

    ARENA arena;
    Arena_Init(&arena, "MPN test", 1024);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    MPN tiles;
    const bool loaded = Result_Log(
        Mpn_Load(&reader, Arena_Allocator(&arena), &tiles), LOG_LEVEL_ERROR);
    CHECK(loaded);
    if (loaded) {
        CHECK_EQ_INT(tiles.count, 2);
        CHECK_EQ_INT(tiles.pixels[0], 1);
        CHECK_EQ_INT(tiles.pixels[8], 2);
        CHECK_EQ_INT(tiles.pixels[15 * 16 + 7], 4);
        CHECK_EQ_INT(tiles.pixels[255], 8);
        CHECK_EQ_INT(tiles.pixels[1], 0);
        for (size_t i = 256; i < 512; i++) {
            CHECK_EQ_INT(tiles.pixels[i], 15);
        }
        CHECK_EQ_INT(tiles.palette[1].red, 13);
        CHECK_EQ_INT(tiles.palette[1].green, 7);
        CHECK_EQ_INT(tiles.palette[1].blue, 3);
        CHECK_EQ_INT(tiles.palette[0].alpha, 255);
        CHECK_EQ_INT(Reader_Tell(&reader), sizeof(bytes));
    }
    Reader_Close(&reader);
    memset(bytes, 0, sizeof(bytes));
    if (loaded) {
        CHECK_EQ_INT(tiles.pixels[511], 15);
    }
    Arena_Free(&arena);
}

TEST(rejects_every_truncated_prefix_without_arena_allocations)
{
    uint8_t bytes[54 + 128] = {};
    M_Header(bytes, 1);
    ARENA arena;
    Arena_Init(&arena, "MPN test", 1024);
    for (size_t size = 0; size < sizeof(bytes); size++) {
        READER reader;
        Reader_OpenBuffer(&reader, bytes, size, nullptr);
        MPN tiles = { .count = 123 };
        CHECK(
            !Result_Ignore(Mpn_Load(&reader, Arena_Allocator(&arena), &tiles)));
        CHECK_EQ_INT(tiles.count, 0);
        CHECK(tiles.pixels == nullptr);
        CHECK_EQ_INT(arena.used, 0);
        Reader_Close(&reader);
    }
    Arena_Free(&arena);
}

TEST(rejects_bad_signature_without_arena_allocations)
{
    uint8_t bytes[54 + 128] = {};
    M_Header(bytes, 1);
    bytes[3] = 'X';
    ARENA arena;
    Arena_Init(&arena, "MPN test", 1024);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    MPN tiles;
    CHECK(!Result_Ignore(Mpn_Load(&reader, Arena_Allocator(&arena), &tiles)));
    CHECK(tiles.pixels == nullptr);
    CHECK_EQ_INT(arena.used, 0);
    Reader_Close(&reader);
    Arena_Free(&arena);
}

TEST(loads_all_256_tiles_and_leaves_trailing_bytes_unread)
{
    uint8_t *const bytes = Mem_AllocZ(54 + 256 * 128 + 1);
    M_Header(bytes, 256);
    for (size_t tile = 0; tile < 256; tile++) {
        for (size_t plane = 0; plane < 4; plane++) {
            memset(
                bytes + 54 + tile * 128 + plane * 32,
                (tile & (1u << plane)) != 0 ? 255 : 0, 32);
        }
    }
    bytes[54 + 256 * 128] = 0x5a;
    ARENA arena;
    Arena_Init(&arena, "MPN test", 65536);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, 54 + 256 * 128 + 1, nullptr);
    MPN tiles;
    const bool loaded = Result_Log(
        Mpn_Load(&reader, Arena_Allocator(&arena), &tiles), LOG_LEVEL_ERROR);
    CHECK(loaded);
    if (loaded) {
        CHECK_EQ_INT(tiles.count, 256);
        for (size_t tile = 0; tile < 256; tile++) {
            for (size_t pixel = 0; pixel < 256; pixel++) {
                CHECK_EQ_INT(tiles.pixels[tile * 256 + pixel], tile & 15);
            }
        }
        CHECK_EQ_INT(Reader_ReadU8(&reader), 0x5a);
        CHECK_EQ_INT(Reader_Remaining(&reader), 0);
    }
    Reader_Close(&reader);
    Arena_Free(&arena);
    Mem_Free(bytes);
}
