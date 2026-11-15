#include "th01/bfnt.h"

#include "core/base/arena.h"

#include "test.h"

static void M_File(uint8_t bytes[96])
{
    memset(bytes, 0, 96);
    memcpy(bytes, "BFNT\x1a", 5);
    bytes[5] = 0x83;
    bytes[6] = 0x16;
    bytes[8] = 8;
    bytes[10] = 2;
    bytes[14] = 1;
    bytes[35] = 0x60;
    bytes[36] = 0x90;
    bytes[37] = 0xf0;
    bytes[80] = 0x1f;
    bytes[88] = 0x23;
}

TEST(loads_packed_indices_and_blue_red_green_palette)
{
    ARENA arena;
    Arena_Init(&arena, "BFNT test", 1024);
    uint8_t bytes[96];
    M_File(bytes);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    BFNT sprites;
    CHECK(Result_Log(
        Bfnt_Load(&reader, Arena_Allocator(&arena), &sprites),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(sprites.width, 8);
    CHECK_EQ_INT(sprites.height, 2);
    CHECK_EQ_INT(sprites.count, 2);
    CHECK_EQ_INT(sprites.pixels[0], 1);
    CHECK_EQ_INT(sprites.pixels[1], 15);
    CHECK_EQ_INT(sprites.pixels[16], 2);
    CHECK_EQ_INT(sprites.pixels[17], 3);
    CHECK_EQ_INT(sprites.palette[1].blue, 0x60);
    CHECK_EQ_INT(sprites.palette[1].red, 0x90);
    CHECK_EQ_INT(sprites.palette[1].green, 0xf0);
    CHECK_EQ_INT(sprites.palette[0].alpha, 0);
    Reader_Close(&reader);
    Arena_Free(&arena);
}

TEST(rejects_every_truncated_prefix_without_allocating)
{
    ARENA arena;
    Arena_Init(&arena, "BFNT test", 1024);
    uint8_t bytes[96];
    M_File(bytes);
    for (size_t size = 0; size < sizeof(bytes); size++) {
        READER reader;
        Reader_OpenBuffer(&reader, bytes, size, nullptr);
        BFNT sprites;
        CHECK(!Result_Ignore(
            Bfnt_Load(&reader, Arena_Allocator(&arena), &sprites)));
        CHECK(sprites.pixels == nullptr);
        CHECK_EQ_INT(arena.used, 0);
        Reader_Close(&reader);
    }
    Arena_Free(&arena);
}

TEST(rejects_unsupported_or_invalid_headers)
{
    ARENA arena;
    Arena_Init(&arena, "BFNT test", 1024);
    const int32_t offsets[] = { 0, 5, 8, 9, 10, 12, 15, 28 };
    const uint8_t values[] = { 'X', 3, 7, 255, 0, 1, 255, 1 };
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        uint8_t bytes[96];
        M_File(bytes);
        bytes[offsets[i]] = values[i];
        READER reader;
        Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
        BFNT sprites;
        CHECK(!Result_Ignore(
            Bfnt_Load(&reader, Arena_Allocator(&arena), &sprites)));
        CHECK(sprites.pixels == nullptr);
        CHECK_EQ_INT(arena.used, 0);
        Reader_Close(&reader);
    }
    Arena_Free(&arena);
}

TEST(assets_share_an_arena_and_survive_reader_close)
{
    ARENA arena;
    Arena_Init(&arena, "BFNT assets", 1024);
    uint8_t bytes[96];
    M_File(bytes);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    BFNT first, second;
    CHECK(Result_Log(
        Bfnt_Load(&reader, Arena_Allocator(&arena), &first), LOG_LEVEL_ERROR));
    Reader_Close(&reader);
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    CHECK(Result_Log(
        Bfnt_Load(&reader, Arena_Allocator(&arena), &second), LOG_LEVEL_ERROR));
    Reader_Close(&reader);
    memset(bytes, 0, sizeof(bytes));
    CHECK(first.pixels != second.pixels);
    CHECK_EQ_INT(first.pixels[1], 15);
    CHECK_EQ_INT(second.pixels[17], 3);
    CHECK_EQ_INT(arena.used, 64);
    Arena_Reset(&arena);
    CHECK_EQ_INT(arena.used, 0);
    Arena_Free(&arena);
}
