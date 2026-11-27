#include "tests/test.h"
#include "th01/grc.h"

#include "core/base/arena.h"
#include "core/io/file.h"
#include "core/io/reader.h"

#include <stdio.h>
#include <string.h>

static size_t M_File(uint8_t *const bytes, const int32_t count)
{
    const size_t size = 64 + (size_t)count * 6;
    memset(bytes, 0, size);
    memcpy(bytes, "GRCG", 4);
    bytes[4] = 2;
    bytes[6] = 3;
    bytes[8] = count;
    memset(bytes + 16, 0xff, 48);
    for (int32_t i = 0; i < count; i++) {
        const uint8_t patterns[6] = { 0x80, 0x01, 0x55, 0xaa, 0x00, 0xff };
        for (int32_t j = 0; j < 6; j++) {
            bytes[64 + i * 6 + j] = patterns[(i + j) % 6];
        }
    }
    return size;
}

static RESULT M_Load(
    const uint8_t *const bytes, const size_t size,
    const ALLOCATOR *const allocator, GRC *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    return Grc_Load(&reader, allocator, out);
}

TEST(loads_cross_byte_masks_and_multiple_images)
{
    ARENA arena;
    Arena_Init(&arena, "GRC test", 1024 * 1024);
    uint8_t bytes[64 + 2 * 6];
    const size_t size = M_File(bytes, 2);
    GRC sprites;
    CHECK(Result_Log(
        M_Load(bytes, size, Arena_Allocator(&arena), &sprites),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(sprites.width, 16);
    CHECK_EQ_INT(sprites.height, 3);
    CHECK_EQ_INT(sprites.count, 2);
    if (sprites.coverage != nullptr) {
        const uint8_t expected[] = {
            1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
            0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
        };
        CHECK(memcmp(sprites.coverage, expected, sizeof(expected)) == 0);
        CHECK_EQ_INT(sprites.coverage[48], 0);
        CHECK_EQ_INT(sprites.coverage[55], 1);
        CHECK_EQ_INT(sprites.coverage[56], 0);
        CHECK_EQ_INT(sprites.coverage[57], 1);
    }
    Arena_Free(&arena);
}

TEST(loads_all_eight_images_and_ignores_palette_bytes)
{
    ARENA arena;
    Arena_Init(&arena, "GRC test", 1024 * 1024);
    uint8_t bytes[64 + GRC_MAX_IMAGES * 6];
    const size_t size = M_File(bytes, GRC_MAX_IMAGES);
    GRC sprites;
    CHECK(Result_Log(
        M_Load(bytes, size, Arena_Allocator(&arena), &sprites),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(sprites.count, GRC_MAX_IMAGES);
    if (sprites.coverage != nullptr) {
        CHECK_EQ_INT(sprites.coverage[7 * 48 + 7], 1);
    }
    Arena_Free(&arena);
}

TEST(owns_masks_after_reader_close_and_does_not_consume_trailing_data)
{
    ARENA arena;
    Arena_Init(&arena, "GRC test", 1024 * 1024);
    uint8_t bytes[64 + 6 + 1];
    const size_t size = M_File(bytes, 1);
    bytes[size] = 0x5a;
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    GRC sprites;
    CHECK(Result_Log(
        Grc_Load(&reader, Arena_Allocator(&arena), &sprites), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Reader_ReadU8(&reader), 0x5a);
    Reader_Close(&reader);
    memset(bytes, 0, sizeof(bytes));
    if (sprites.coverage != nullptr) {
        CHECK_EQ_INT(sprites.coverage[0], 1);
        CHECK_EQ_INT(sprites.coverage[15], 1);
    }
    Arena_Free(&arena);
}

TEST(rejects_every_truncated_prefix)
{
    ARENA arena;
    Arena_Init(&arena, "GRC test", 1024 * 1024);
    uint8_t bytes[64 + 2 * 6];
    const size_t size = M_File(bytes, 2);
    for (size_t prefix = 0; prefix < size; prefix++) {
        GRC sprites;
        CHECK(!Result_Ignore(
            M_Load(bytes, prefix, Arena_Allocator(&arena), &sprites)));
        CHECK(sprites.coverage == nullptr);
        CHECK_EQ_INT(arena.used, 0);
        CHECK_EQ_INT(sprites.count, 0);
    }
    Arena_Free(&arena);
}

TEST(rejects_invalid_signature_dimensions_and_counts)
{
    ARENA arena;
    Arena_Init(&arena, "GRC test", 1024 * 1024);
    const int32_t offsets[] = { 0, 4, 5, 6, 7, 8, 8 };
    const uint8_t values[] = { 'X', 0, 0xff, 0, 0xff, 0, 9 };
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        uint8_t bytes[64 + 6];
        const size_t size = M_File(bytes, 1);
        bytes[offsets[i]] = values[i];
        GRC sprites;
        CHECK(!Result_Ignore(
            M_Load(bytes, size, Arena_Allocator(&arena), &sprites)));
        CHECK(sprites.coverage == nullptr);
        CHECK_EQ_INT(arena.used, 0);
    }
    Arena_Free(&arena);
}

TEST(rejects_legacy_size_overflow_before_allocating)
{
    ARENA arena;
    Arena_Init(&arena, "GRC test", 1024 * 1024);
    uint8_t bytes[64 + 6];
    const size_t size = M_File(bytes, 1);
    bytes[4] = 0;
    bytes[5] = 1;
    bytes[6] = 0;
    bytes[7] = 1;
    GRC sprites;
    CHECK(
        !Result_Ignore(M_Load(bytes, size, Arena_Allocator(&arena), &sprites)));
    CHECK(sprites.coverage == nullptr);
    CHECK_EQ_INT(arena.used, 0);
    Arena_Free(&arena);
}

TEST(assets_share_an_arena_and_release_together)
{
    ARENA arena;
    Arena_Init(&arena, "GRC assets", 4096);
    uint8_t bytes[64 + 6];
    const size_t size = M_File(bytes, 1);
    GRC first;
    GRC second;
    CHECK(Result_Ignore(M_Load(bytes, size, Arena_Allocator(&arena), &first)));
    CHECK(Result_Ignore(M_Load(bytes, size, Arena_Allocator(&arena), &second)));
    memset(bytes, 0, sizeof(bytes));
    CHECK(first.coverage != second.coverage);
    CHECK_EQ_INT(first.coverage[0], 1);
    CHECK_EQ_INT(second.coverage[15], 1);
    CHECK_EQ_INT(arena.used, 96);
    Arena_Reset(&arena);
    CHECK_EQ_INT(arena.used, 0);
    Arena_Free(&arena);
}

// Windows prevents a file opened by READER from shrinking.
#ifndef _WIN32
TEST(a_late_file_read_failure_keeps_the_asset_arena_unchanged)
{
    const char *const path = "test_th01_grc_truncated.bin";
    uint8_t bytes[64 + 3 * 32000];
    memset(bytes, 0xa5, sizeof(bytes));
    memcpy(bytes, "GRCG", 4);
    bytes[4] = 80;
    bytes[5] = 0;
    bytes[6] = 0x90;
    bytes[7] = 1;
    bytes[8] = 3;
    FILE *file = fopen(path, "wb");
    CHECK(file != nullptr);
    if (file == nullptr) {
        return;
    }
    CHECK_EQ_INT(fwrite(bytes, 1, sizeof(bytes), file), sizeof(bytes));
    CHECK_EQ_INT(fclose(file), 0);
    READER reader;
    const bool opened =
        Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR);
    CHECK(opened);
    if (!opened) {
        CHECK(Result_Ignore(File_Remove(path)));
        return;
    }
    CHECK_EQ_INT(Reader_ReadU8(&reader), 'G');
    Reader_Seek(&reader, 0);
    file = fopen(path, "wb");
    CHECK(file != nullptr);
    if (file == nullptr) {
        Reader_Close(&reader);
        CHECK(Result_Ignore(File_Remove(path)));
        return;
    }
    CHECK_EQ_INT(fclose(file), 0);
    ARENA arena;
    Arena_Init(&arena, "GRC assets", 1024 * 1024);
    uint8_t *const sentinel = Arena_Alloc(&arena, 16);
    memset(sentinel, 0x3c, 16);
    const size_t used = arena.used;
    GRC sprites;
    CHECK(!Result_Ignore(Grc_Load(&reader, Arena_Allocator(&arena), &sprites)));
    CHECK(sprites.coverage == nullptr);
    CHECK_EQ_INT(sprites.count, 0);
    CHECK_EQ_INT(arena.used, used);
    CHECK_EQ_INT(sentinel[0], 0x3c);
    CHECK_EQ_INT(sentinel[15], 0x3c);
    Reader_Close(&reader);
    Arena_Free(&arena);
    CHECK(Result_Ignore(File_Remove(path)));
}
#endif
