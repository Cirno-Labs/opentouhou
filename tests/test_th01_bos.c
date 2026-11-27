#include "th01/bos.h"

#include "core/base/arena.h"
#include "core/io/file.h"

#include "test.h"

static uint8_t M_Color(const size_t pixel)
{
    return (pixel * 7 + pixel / 11) % 16;
}

static uint8_t M_Coverage(const size_t pixel)
{
    return (pixel / 3 + pixel / 17) % 2;
}

static size_t M_File(
    uint8_t *const bytes, const int32_t width_bytes, const int32_t height,
    const int32_t count)
{
    const size_t plane_size = (size_t)width_bytes * (size_t)height;
    const size_t size = 64 + (size_t)count * plane_size * 5;
    memset(bytes, 0, size);
    memcpy(bytes, "BOSS", 4);
    bytes[4] = width_bytes;
    bytes[5] = 0xff;
    bytes[6] = height;
    bytes[7] = 0xa5;
    bytes[8] = count;
    memset(bytes + 9, 0xa5, 55);
    for (int32_t image = 0; image < count; image++) {
        for (size_t pixel = 0; pixel < plane_size * 8; pixel++) {
            const size_t absolute = (size_t)image * plane_size * 8 + pixel;
            const uint8_t color = M_Color(absolute);
            const uint8_t mask = 0x80 >> (pixel % 8);
            const size_t byte = 64 + (size_t)image * plane_size * 5 + pixel / 8;
            if (M_Coverage(absolute) == 0) {
                bytes[byte] |= mask;
            }
            for (int32_t plane = 0; plane < 4; plane++) {
                if ((color & (1 << plane)) != 0) {
                    bytes[byte + (size_t)(plane + 1) * plane_size] |= mask;
                }
            }
        }
    }
    return size;
}

static RESULT M_Load(
    const uint8_t *const bytes, const size_t size,
    const ALLOCATOR *const allocator, BOS *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, nullptr);
    return Bos_Load(&reader, allocator, out);
}

TEST(preserves_plane_order_and_independent_coverage)
{
    const uint8_t bytes[69] = {
        'B', 'O', 'S', 'S', 1, 0, 1, 0, 1, [64] = 0xa0, 0x38, 0x34, 0x32, 0x31,
    };
    const uint8_t colors[8] = { 0, 0, 15, 15, 1, 2, 4, 8 };
    const uint8_t coverage[8] = { 0, 1, 0, 1, 1, 1, 1, 1 };
    ARENA arena;
    Arena_Init(&arena, "BOS test", 4096);
    BOS sprites;
    CHECK(Result_Log(
        M_Load(bytes, sizeof(bytes), Arena_Allocator(&arena), &sprites),
        LOG_LEVEL_ERROR));
    if (sprites.pixels != nullptr) {
        CHECK(memcmp(sprites.pixels, colors, 8) == 0);
        CHECK(memcmp(sprites.coverage, coverage, 8) == 0);
        CHECK(sprites.coverage == sprites.pixels + 8);
    }
    CHECK_EQ_INT(arena.used, 16);
    Arena_Free(&arena);
}

TEST(loads_odd_byte_widths_all_frames_and_full_header_ranges)
{
    const int32_t dimensions[][3] = {
        { 3, 3, 2 },
        { 6, 48, 28 },
        { 255, 255, 1 },
        { 1, 1, 255 },
    };
    ARENA staging;
    ARENA assets;
    Arena_Init(&staging, "BOS synthetic", 1024 * 1024);
    Arena_Init(&assets, "BOS assets", 2 * 1024 * 1024);
    for (size_t i = 0; i < sizeof(dimensions) / sizeof(dimensions[0]); i++) {
        const int32_t width = dimensions[i][0];
        const int32_t height = dimensions[i][1];
        const int32_t count = dimensions[i][2];
        const size_t pixels =
            (size_t)width * (size_t)height * (size_t)count * 8;
        uint8_t *const bytes = Arena_Alloc(&staging, 64 + pixels / 8 * 5);
        const size_t size = M_File(bytes, width, height, count);
        BOS sprites;
        CHECK(Result_Log(
            M_Load(bytes, size, Arena_Allocator(&assets), &sprites),
            LOG_LEVEL_ERROR));
        CHECK_EQ_INT(sprites.width, width * 8);
        CHECK_EQ_INT(sprites.height, height);
        CHECK_EQ_INT(sprites.count, count);
        size_t mismatches = 0;
        if (sprites.pixels != nullptr) {
            for (size_t pixel = 0; pixel < pixels; pixel++) {
                mismatches += sprites.pixels[pixel] != M_Color(pixel);
                mismatches += sprites.coverage[pixel] != M_Coverage(pixel);
            }
        }
        CHECK_EQ_INT(mismatches, 0);
        CHECK_EQ_INT(assets.used, pixels * 2);
        Arena_Reset(&staging);
        Arena_Reset(&assets);
    }
    Arena_Free(&assets);
    Arena_Free(&staging);
}

TEST(owns_data_after_reader_close_and_respects_reader_position)
{
    ARENA arena;
    Arena_Init(&arena, "BOS assets", 4096);
    uint8_t bytes[5 + 64 + 5 + 1] = {};
    const size_t size = M_File(bytes + 5, 1, 1, 1);
    bytes[5 + size] = 0x5a;
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes), nullptr);
    Reader_Skip(&reader, 5);
    BOS first;
    BOS second;
    CHECK(Result_Log(
        Bos_Load(&reader, Arena_Allocator(&arena), &first), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Reader_Tell(&reader), 5 + size);
    CHECK_EQ_INT(Reader_ReadU8(&reader), 0x5a);
    Reader_Close(&reader);
    CHECK(Result_Ignore(
        M_Load(bytes + 5, size, Arena_Allocator(&arena), &second)));
    memset(bytes, 0, sizeof(bytes));
    CHECK(first.pixels != second.pixels);
    CHECK_EQ_INT(arena.used, 32);
    if (first.pixels != nullptr && second.pixels != nullptr) {
        CHECK_EQ_INT(first.pixels[7], M_Color(7));
        CHECK_EQ_INT(second.coverage[7], M_Coverage(7));
    }
    Arena_Reset(&arena);
    CHECK_EQ_INT(arena.used, 0);
    Arena_Free(&arena);
}

TEST(rejects_every_truncated_prefix_without_allocating_assets)
{
    ARENA arena;
    Arena_Init(&arena, "BOS assets", 4096);
    uint8_t bytes[64 + 2 * 3 * 3 * 5];
    const size_t size = M_File(bytes, 3, 3, 2);
    for (size_t prefix = 0; prefix < size; prefix++) {
        BOS sprites = { .count = 42 };
        CHECK(!Result_Ignore(
            M_Load(bytes, prefix, Arena_Allocator(&arena), &sprites)));
        CHECK(sprites.pixels == nullptr);
        CHECK(sprites.coverage == nullptr);
        CHECK_EQ_INT(sprites.width, 0);
        CHECK_EQ_INT(sprites.height, 0);
        CHECK_EQ_INT(sprites.count, 0);
        CHECK_EQ_INT(arena.used, 0);
    }
    Arena_Free(&arena);
}

TEST(rejects_bad_signature_and_zero_dimensions_or_count)
{
    ARENA arena;
    Arena_Init(&arena, "BOS assets", 4096);
    const int32_t offsets[] = { 0, 1, 2, 3, 4, 6, 8 };
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        uint8_t bytes[69];
        const size_t size = M_File(bytes, 1, 1, 1);
        bytes[offsets[i]] = 0;
        BOS sprites;
        CHECK(!Result_Ignore(
            M_Load(bytes, size, Arena_Allocator(&arena), &sprites)));
        CHECK(sprites.pixels == nullptr);
        CHECK(sprites.coverage == nullptr);
        CHECK_EQ_INT(arena.used, 0);
    }
    Arena_Free(&arena);
}

// Windows prevents a file opened by READER from shrinking.
#ifndef _WIN32
TEST(late_io_failure_preserves_existing_arena_assets)
{
    const char *const path = "test_th01_bos_truncated.bin";
    ARENA staging;
    ARENA assets;
    Arena_Init(&staging, "BOS synthetic", 512 * 1024);
    Arena_Init(&assets, "BOS assets", 4096);
    const size_t file_size = 64 + 255 * 255 * 5;
    uint8_t *const file_bytes = Arena_Alloc(&staging, file_size);
    M_File(file_bytes, 255, 255, 1);
    FILE *file = fopen(path, "wb");
    CHECK(file != nullptr);
    if (file == nullptr) {
        Arena_Free(&assets);
        Arena_Free(&staging);
        return;
    }
    CHECK_EQ_INT(fwrite(file_bytes, 1, file_size, file), file_size);
    CHECK_EQ_INT(fclose(file), 0);
    READER reader;
    const bool opened =
        Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR);
    CHECK(opened);
    if (opened) {
        CHECK_EQ_INT(Reader_ReadU8(&reader), 'B');
        Reader_Seek(&reader, 0);
        file = fopen(path, "wb");
        CHECK(file != nullptr);
        if (file != nullptr) {
            CHECK_EQ_INT(fclose(file), 0);
            uint8_t *const sentinel = Arena_Alloc(&assets, 16);
            memset(sentinel, 0x3c, 16);
            const size_t used = assets.used;
            BOS sprites;
            CHECK(!Result_Ignore(
                Bos_Load(&reader, Arena_Allocator(&assets), &sprites)));
            CHECK(sprites.pixels == nullptr);
            CHECK(sprites.coverage == nullptr);
            CHECK_EQ_INT(sprites.count, 0);
            CHECK_EQ_INT(assets.used, used);
            CHECK_EQ_INT(sentinel[0], 0x3c);
            CHECK_EQ_INT(sentinel[15], 0x3c);
        }
        Reader_Close(&reader);
    }
    Arena_Free(&assets);
    Arena_Free(&staging);
    CHECK(Result_Ignore(File_Remove(path)));
}
#endif
