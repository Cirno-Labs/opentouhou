#include "tests/test.h"
#include "th01/grz.h"

#include "core/base/arena.h"
#include "core/io/reader.h"

#include <string.h>

#define M_CAPACITY (16 * 1024 * 1024)
#define M_PIXELS (GRX_WIDTH * GRX_HEIGHT)
#define M_CELLS (M_PIXELS / 8)

typedef struct {
    uint8_t bytes[16384];
    size_t size;
} GRZ_BUILDER;

static void M_Byte(GRZ_BUILDER *const file, const uint8_t byte)
{
    file->bytes[file->size++] = byte;
}

static void M_Word(GRZ_BUILDER *const file, const uint16_t value)
{
    M_Byte(file, value);
    M_Byte(file, value >> 8);
}

static void M_Dword(GRZ_BUILDER *const file, const uint32_t value)
{
    M_Word(file, value);
    M_Word(file, value >> 16);
}

static void M_SetDword(
    GRZ_BUILDER *const file, const size_t offset, const uint32_t value)
{
    for (int32_t i = 0; i < 4; i++) {
        file->bytes[offset + i] = value >> (i * 8);
    }
}

static void M_Bytes(
    GRZ_BUILDER *const file, const void *const data, const size_t size)
{
    memcpy(file->bytes + file->size, data, size);
    file->size += size;
}

static void M_Skip(GRZ_BUILDER *const rle, size_t cells)
{
    while (cells != 0) {
        const size_t count = cells < 256 ? cells : 256;
        if (count == 1) {
            M_Byte(rle, 2);
        } else {
            M_Byte(rle, 1);
            M_Byte(rle, count - 1);
            M_Byte(rle, 2);
        }
        cells -= count;
    }
}

static GRZ_BUILDER M_Frame(
    const int32_t streams, const GRZ_BUILDER *const rle,
    const uint8_t *const planes, const uint16_t planar_size)
{
    GRZ_BUILDER frame = {};
    M_Bytes(&frame, "HGRX", 4);
    M_Byte(&frame, streams);
    for (int32_t i = 0; i < 3; i++) {
        M_Byte(&frame, 0);
    }
    M_Word(&frame, rle->size);
    M_Word(&frame, planar_size);
    M_Dword(&frame, 0);
    for (int32_t i = 0; i < 16; i++) {
        M_Byte(&frame, i);
        M_Byte(&frame, 15 - i);
        M_Byte(&frame, i / 2);
    }
    M_Bytes(&frame, rle->bytes, rle->size);
    if (planar_size != 0) {
        M_Bytes(&frame, planes, (size_t)streams * planar_size);
    }
    return frame;
}

static GRZ_BUILDER M_Container(
    const GRZ_BUILDER *const frames, const int32_t count)
{
    GRZ_BUILDER file = {};
    M_Bytes(&file, "HGRZ", 4);
    M_Byte(&file, count);
    for (int32_t i = 0; i < 3; i++) {
        M_Byte(&file, 0);
    }
    for (int32_t i = 0; i < 16; i++) {
        M_Dword(&file, 0);
    }
    M_Dword(&file, 0);
    for (int32_t i = 0; i < 20; i++) {
        M_Byte(&file, 0);
    }
    for (int32_t i = 0; i < count; i++) {
        M_SetDword(&file, 8 + i * 4, file.size);
        M_Bytes(&file, frames[i].bytes, frames[i].size);
    }
    M_SetDword(&file, 72, file.size);
    return file;
}

static RESULT M_LoadFrame(
    const GRZ_BUILDER *const file, ARENA *const arena, GRX *const image)
{
    READER reader;
    Reader_OpenBuffer(&reader, file->bytes, file->size, nullptr);
    return Grx_Load(&reader, Arena_Allocator(arena), image);
}

static RESULT M_Load(
    const GRZ_BUILDER *const file, ARENA *const arena, GRZ *const images)
{
    READER reader;
    Reader_OpenBuffer(&reader, file->bytes, file->size, nullptr);
    return Grz_Load(&reader, Arena_Allocator(arena), images);
}

static GRZ_BUILDER M_OneCell(const uint8_t planes[4])
{
    GRZ_BUILDER rle = {};
    M_Byte(&rle, 0);
    M_Skip(&rle, M_CELLS - 1);
    return M_Frame(1, &rle, planes, 4);
}

static void M_CheckEmpty(const GRX *const image)
{
    CHECK_EQ_INT(image->stream_count, 0);
    CHECK(image->coverage == nullptr);
    CHECK(image->pixels == nullptr);
}

TEST(decodes_all_colors_and_keeps_opaque_zero_separate_from_skips)
{
    GRZ_BUILDER rle = {};
    M_Byte(&rle, 0);
    M_Byte(&rle, 0);
    M_Skip(&rle, M_CELLS - 2);
    const uint8_t planes[] = { 0x55, 0x33, 0x0f, 0, 0x55, 0x33, 0x0f, 0xff };
    const GRZ_BUILDER frame = M_Frame(1, &rle, planes, sizeof(planes));
    ARENA arena;
    Arena_Init(&arena, "grx colors", M_CAPACITY);
    GRX image;
    CHECK(Result_Log(M_LoadFrame(&frame, &arena, &image), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(image.stream_count, 1);
    for (int32_t i = 0; i < 16; i++) {
        CHECK_EQ_INT(image.pixels[i], i);
        CHECK_EQ_INT(image.coverage[i], 1);
    }
    CHECK_EQ_INT(image.pixels[16], 0);
    CHECK_EQ_INT(image.coverage[16], 0);
    CHECK_EQ_INT(image.palette[4].red, 68);
    CHECK_EQ_INT(image.palette[4].green, 187);
    CHECK_EQ_INT(image.palette[4].blue, 34);
    CHECK_EQ_INT(image.palette[0].alpha, 255);
    Arena_Free(&arena);
}

TEST(zero_count_run_skips_one_cell_without_reading_a_command)
{
    GRZ_BUILDER rle = {};
    M_Byte(&rle, 1);
    M_Byte(&rle, 0);
    M_Byte(&rle, 0);
    M_Skip(&rle, M_CELLS - 2);
    const uint8_t planes[] = { 0xff, 0, 0, 0 };
    const GRZ_BUILDER frame = M_Frame(1, &rle, planes, sizeof(planes));
    ARENA arena;
    Arena_Init(&arena, "grx zero run", M_CAPACITY);
    GRX image;
    CHECK(Result_Log(M_LoadFrame(&frame, &arena, &image), LOG_LEVEL_ERROR));
    for (int32_t i = 0; i < 8; i++) {
        CHECK_EQ_INT(image.coverage[i], 0);
        CHECK_EQ_INT(image.coverage[i + 8], 1);
        CHECK_EQ_INT(image.pixels[i + 8], 1);
    }
    Arena_Free(&arena);
}

TEST(nonzero_runs_include_the_common_final_put_or_skip)
{
    GRZ_BUILDER rle = {};
    const uint8_t commands[] = { 1, 1, 0, 1, 2, 5, 0 };
    M_Bytes(&rle, commands, sizeof(commands));
    M_Skip(&rle, M_CELLS - 6);
    const uint8_t planes[] = {
        0xff, 0, 0, 0, 0, 0xff, 0, 0, 0, 0, 0xff, 0,
    };
    const GRZ_BUILDER frame = M_Frame(1, &rle, planes, sizeof(planes));
    ARENA arena;
    Arena_Init(&arena, "grx runs", M_CAPACITY);
    GRX image;
    CHECK(Result_Log(M_LoadFrame(&frame, &arena, &image), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(image.pixels[0], 1);
    CHECK_EQ_INT(image.pixels[8], 2);
    for (int32_t cell = 2; cell < 5; cell++) {
        CHECK_EQ_INT(image.coverage[cell * 8], 0);
    }
    CHECK_EQ_INT(image.pixels[40], 4);
    CHECK_EQ_INT(image.coverage[48], 0);
    Arena_Free(&arena);
}

TEST(streams_share_coverage_but_keep_independent_planar_data)
{
    GRZ_BUILDER rle = {};
    M_Byte(&rle, 0);
    M_Skip(&rle, M_CELLS - 1);
    const uint8_t planes[] = { 0xff, 0, 0, 0, 0, 0, 0, 0xff };
    const GRZ_BUILDER frame = M_Frame(2, &rle, planes, 4);
    ARENA arena;
    Arena_Init(&arena, "grx streams", M_CAPACITY);
    GRX image;
    CHECK(Result_Log(M_LoadFrame(&frame, &arena, &image), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(image.stream_count, 2);
    CHECK_EQ_INT(image.coverage[0], 1);
    CHECK_EQ_INT(image.coverage[8], 0);
    CHECK_EQ_INT(image.pixels[0], 1);
    CHECK_EQ_INT(image.pixels[M_PIXELS], 8);
    CHECK_EQ_INT(image.pixels[M_PIXELS + 8], 0);
    Arena_Free(&arena);
}

TEST(clips_long_final_runs_to_the_visible_frame)
{
    GRZ_BUILDER rle = {};
    M_Skip(&rle, M_CELLS - 1);
    const uint8_t last[] = { 1, 255, 0 };
    M_Bytes(&rle, last, sizeof(last));
    const uint8_t planes[] = { 0, 0xff, 0xff, 0xff };
    const GRZ_BUILDER frame = M_Frame(1, &rle, planes, sizeof(planes));
    ARENA arena;
    Arena_Init(&arena, "grx edge", M_CAPACITY);
    GRX image;
    CHECK(Result_Log(M_LoadFrame(&frame, &arena, &image), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(image.coverage[M_PIXELS - 9], 0);
    CHECK_EQ_INT(image.coverage[M_PIXELS - 1], 1);
    CHECK_EQ_INT(image.pixels[M_PIXELS - 1], 14);
    Arena_Free(&arena);
}

TEST(loads_unordered_and_duplicate_frame_offsets)
{
    const uint8_t first[] = { 0xff, 0, 0, 0 };
    const uint8_t second[] = { 0, 0xff, 0, 0 };
    const GRZ_BUILDER frames[] = { M_OneCell(first), M_OneCell(second) };
    GRZ_BUILDER file = M_Container(frames, 2);
    const uint32_t second_offset = 96 + frames[0].size;
    M_SetDword(&file, 8, second_offset);
    M_SetDword(&file, 12, 96);
    ARENA arena;
    Arena_Init(&arena, "grz offsets", M_CAPACITY);
    GRZ images;
    CHECK(Result_Log(M_Load(&file, &arena, &images), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(images.count, 2);
    CHECK_EQ_INT(images.images[0].pixels[0], 2);
    CHECK_EQ_INT(images.images[1].pixels[0], 1);
    Arena_Reset(&arena);
    M_SetDword(&file, 12, second_offset);
    CHECK(Result_Log(M_Load(&file, &arena, &images), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(images.images[1].pixels[0], 2);
    CHECK(images.images[0].pixels != images.images[1].pixels);
    Arena_Free(&arena);
}

TEST(supports_sixteen_frames_and_sixteen_streams)
{
    GRZ_BUILDER rle = {};
    M_Skip(&rle, M_CELLS);
    GRZ_BUILDER frames[16];
    for (int32_t i = 0; i < 16; i++) {
        frames[i] = M_Frame(1, &rle, nullptr, 0);
    }
    const GRZ_BUILDER file = M_Container(frames, 16);
    ARENA arena;
    Arena_Init(&arena, "grz capacities", M_CAPACITY);
    GRZ images;
    CHECK(Result_Log(M_Load(&file, &arena, &images), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(images.count, 16);
    CHECK_EQ_INT(images.images[15].coverage[M_PIXELS - 1], 0);
    Arena_Reset(&arena);
    const GRZ_BUILDER frame = M_Frame(16, &rle, nullptr, 0);
    GRX image;
    CHECK(Result_Log(M_LoadFrame(&frame, &arena, &image), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(image.stream_count, 16);
    CHECK_EQ_INT(image.pixels[15 * M_PIXELS], 0);
    Arena_Free(&arena);
}

TEST(rejects_every_truncated_container_prefix_without_using_the_arena)
{
    const uint8_t planes[] = { 0xff, 0, 0, 0 };
    const GRZ_BUILDER frame = M_OneCell(planes);
    const GRZ_BUILDER file = M_Container(&frame, 1);
    ARENA arena;
    Arena_Init(&arena, "grz truncated", M_CAPACITY);
    for (size_t size = 0; size < file.size; size++) {
        READER reader;
        Reader_OpenBuffer(&reader, file.bytes, size, nullptr);
        GRZ images;
        CHECK(!Result_Ignore(
            Grz_Load(&reader, Arena_Allocator(&arena), &images)));
        CHECK_EQ_INT(images.count, 0);
        CHECK(images.images[0].pixels == nullptr);
        CHECK_EQ_INT(arena.used, 0);
    }
    Arena_Free(&arena);
}

TEST(rejects_invalid_container_counts_sizes_and_offsets)
{
    const uint8_t planes[] = { 0xff, 0, 0, 0 };
    const GRZ_BUILDER frame = M_OneCell(planes);
    const GRZ_BUILDER valid = M_Container(&frame, 1);
    const size_t offsets[] = { 0, 4, 4, 8, 8, 72, 72, 72, 96, 100, 100, 112 };
    const uint32_t values[] = {
        'X', 0, 17, 95, 0xffffffff, 95, 0x7fffffff, 0xffffffff, 'X', 0, 17, 16
    };
    ARENA arena;
    Arena_Init(&arena, "grz invalid", M_CAPACITY);
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++) {
        GRZ_BUILDER file = valid;
        if (offsets[i] == 8 || offsets[i] == 72) {
            M_SetDword(&file, offsets[i], values[i]);
        } else {
            file.bytes[offsets[i]] = values[i];
        }
        GRZ images;
        CHECK(!Result_Ignore(M_Load(&file, &arena, &images)));
        CHECK_EQ_INT(images.count, 0);
        CHECK_EQ_INT(arena.used, 0);
    }
    Arena_Free(&arena);
}

TEST(rejects_short_rle_planar_and_late_frames_without_partial_results)
{
    const uint8_t planes[] = { 0xff, 0, 0, 0 };
    GRZ_BUILDER rle = {};
    M_Byte(&rle, 0);
    const GRZ_BUILDER short_rle = M_Frame(1, &rle, planes, 4);
    M_Skip(&rle, M_CELLS - 1);
    const GRZ_BUILDER short_planar = M_Frame(1, &rle, planes, 3);
    const GRZ_BUILDER valid = M_Frame(1, &rle, planes, 4);
    ARENA arena;
    Arena_Init(&arena, "grz short streams", M_CAPACITY);
    GRX image;
    CHECK(!Result_Ignore(M_LoadFrame(&short_rle, &arena, &image)));
    M_CheckEmpty(&image);
    CHECK(!Result_Ignore(M_LoadFrame(&short_planar, &arena, &image)));
    M_CheckEmpty(&image);
    GRZ_BUILDER incomplete_run = {};
    M_Byte(&incomplete_run, 1);
    GRZ_BUILDER frame = M_Frame(1, &incomplete_run, nullptr, 0);
    CHECK(!Result_Ignore(M_LoadFrame(&frame, &arena, &image)));
    M_CheckEmpty(&image);
    M_Byte(&incomplete_run, 1);
    frame = M_Frame(1, &incomplete_run, nullptr, 0);
    CHECK(!Result_Ignore(M_LoadFrame(&frame, &arena, &image)));
    M_CheckEmpty(&image);
    const GRZ_BUILDER frames[] = { valid, short_planar };
    const GRZ_BUILDER file = M_Container(frames, 2);
    GRZ images;
    CHECK(!Result_Ignore(M_Load(&file, &arena, &images)));
    CHECK_EQ_INT(images.count, 0);
    CHECK(images.images[0].pixels == nullptr);
    CHECK_EQ_INT(arena.used, 0);
    Arena_Free(&arena);
}

TEST(keeps_container_relative_offsets_and_trailing_reader_data)
{
    const uint8_t planes[] = { 0xff, 0, 0, 0 };
    const GRZ_BUILDER frame = M_OneCell(planes);
    const GRZ_BUILDER container = M_Container(&frame, 1);
    GRZ_BUILDER file = {};
    const uint8_t prefix[] = { 9, 8, 7, 6, 5, 4, 3 };
    M_Bytes(&file, prefix, sizeof(prefix));
    M_Bytes(&file, container.bytes, container.size);
    M_Byte(&file, 0x77);
    READER reader;
    Reader_OpenBuffer(&reader, file.bytes, file.size, nullptr);
    Reader_Skip(&reader, sizeof(prefix));
    ARENA arena;
    Arena_Init(&arena, "grz reader range", M_CAPACITY);
    GRZ images;
    CHECK(Result_Log(
        Grz_Load(&reader, Arena_Allocator(&arena), &images), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(images.images[0].pixels[0], 1);
    CHECK_EQ_INT(Reader_Tell(&reader), sizeof(prefix) + container.size);
    CHECK_EQ_INT(Reader_ReadU8(&reader), 0x77);
    Reader_Close(&reader);
    Arena_Reset(&arena);
    file = container;
    memmove(file.bytes + 352, file.bytes + 96, file.size - 96);
    memset(file.bytes + 96, 0xe9, 256);
    file.size += 256;
    for (int32_t i = 0; i < 128; i++) {
        M_Byte(&file, 0xb7);
    }
    M_SetDword(&file, 8, 352);
    M_SetDword(&file, 72, file.size);
    Reader_OpenBuffer(&reader, file.bytes, file.size, nullptr);
    CHECK(Result_Log(
        Grz_Load(&reader, Arena_Allocator(&arena), &images), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(images.images[0].pixels[0], 1);
    CHECK_EQ_INT(Reader_Tell(&reader), file.size);
    Reader_Close(&reader);
    Arena_Reset(&arena);
    file = frame;
    M_Byte(&file, 0x55);
    Reader_OpenBuffer(&reader, file.bytes, file.size, nullptr);
    GRX image;
    CHECK(Result_Log(
        Grx_Load(&reader, Arena_Allocator(&arena), &image), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Reader_Tell(&reader), frame.size);
    CHECK_EQ_INT(Reader_ReadU8(&reader), 0x55);
    Reader_Close(&reader);
    Arena_Free(&arena);
}

TEST(results_outlive_readers_and_share_the_asset_arena)
{
    const uint8_t planes[] = { 0xff, 0, 0, 0 };
    GRZ_BUILDER frame = M_OneCell(planes);
    ARENA arena;
    Arena_Init(&arena, "grz lifetime", M_CAPACITY);
    READER reader;
    Reader_OpenBuffer(&reader, frame.bytes, frame.size, nullptr);
    GRX first;
    CHECK(Result_Log(
        Grx_Load(&reader, Arena_Allocator(&arena), &first), LOG_LEVEL_ERROR));
    Reader_Close(&reader);
    memset(frame.bytes, 0, frame.size);
    CHECK_EQ_INT(first.pixels[0], 1);
    frame = M_OneCell(planes);
    GRX second;
    CHECK(Result_Log(M_LoadFrame(&frame, &arena, &second), LOG_LEVEL_ERROR));
    CHECK(first.pixels != second.pixels);
    CHECK_EQ_INT(first.pixels[0], 1);
    CHECK_EQ_INT(arena.used, 4 * M_PIXELS);
    Arena_Reset(&arena);
    CHECK_EQ_INT(arena.used, 0);
    CHECK(Result_Log(M_LoadFrame(&frame, &arena, &second), LOG_LEVEL_ERROR));
    Arena_Free(&arena);
}
