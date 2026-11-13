// SPDX-License-Identifier: GPL-3.0-only

#include "case.h"

#include "core/base/arena.h"
#include "core/base/utils.h"
#include "test.h"

#include <string.h>

#define M_ARENA_SIZE (1024 * 1024)

static const char *const M_START_NAMES[] = { "seed", "difficulty" };
static const char *const M_CALC_NAMES[] = { "x", "y" };

static CASE_HEADER M_MakeHeader(
    ARENA *const arena, const char *const game_id, const char *const info,
    const uint32_t *const start_values)
{
    CASE_HEADER header = {};
    strcpy(header.game_id, game_id);
    header.info = info;
    Array_Init(&header.start_names, Arena_Allocator(arena));
    Array_Init(&header.start, Arena_Allocator(arena));
    for (size_t i = 0; i < ARRAY_SIZE(M_START_NAMES); i++) {
        Array_Append(&header.start_names, M_START_NAMES[i]);
        Array_Append(&header.start, start_values[i]);
    }
    Array_Init(&header.calc_names, Arena_Allocator(arena));
    for (size_t i = 0; i < ARRAY_SIZE(M_CALC_NAMES); i++) {
        Array_Append(&header.calc_names, M_CALC_NAMES[i]);
    }
    return header;
}

TEST(a_case_round_trips_through_a_buffer)
{
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    const uint32_t start_values[] = { 1234, 2 };
    const CASE_HEADER header =
        M_MakeHeader(&arena, "th06", "a test replay", start_values);

    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    CASE_WRITER *const case_writer = Case_OpenWriter(&writer, &header);
    const uint32_t rows[][2] = {
        { 0, 0 },
        { 1, 1 },
        { 1, 1 },
        { 100, 50 },
    };
    for (size_t i = 0; i < ARRAY_SIZE(rows); i++) {
        Case_WriteRow(case_writer, rows[i]);
    }
    CHECK(Result_Log(Case_CloseWriter(case_writer), LOG_LEVEL_ERROR));

    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));

    READER reader;
    Reader_OpenBuffer(&reader, data, size);
    CASE_READER *case_reader;
    CHECK(Result_Log(
        Case_OpenReader(&reader, &arena, &case_reader), LOG_LEVEL_ERROR));

    const CASE_HEADER *const read_header = Case_GetHeader(case_reader);
    CHECK_EQ_STR(read_header->game_id, "th06");
    CHECK_EQ_STR(read_header->info, "a test replay");
    CHECK_EQ_INT(read_header->start_names.count, 2);
    CHECK_EQ_STR(read_header->start_names.items[0], "seed");
    CHECK_EQ_STR(read_header->start_names.items[1], "difficulty");
    CHECK_EQ_INT(read_header->start.items[0], 1234);
    CHECK_EQ_INT(read_header->start.items[1], 2);
    CHECK_EQ_INT(read_header->calc_names.count, 2);
    CHECK_EQ_STR(read_header->calc_names.items[0], "x");
    CHECK_EQ_STR(read_header->calc_names.items[1], "y");

    for (size_t i = 0; i < ARRAY_SIZE(rows); i++) {
        const uint32_t *const row = Case_ReadRow(case_reader);
        CHECK(row != nullptr);
        if (row != nullptr) {
            CHECK_EQ_INT(row[0], rows[i][0]);
            CHECK_EQ_INT(row[1], rows[i][1]);
        }
    }
    CHECK(Case_ReadRow(case_reader) == nullptr);
    CHECK(Result_Log(Case_CloseReader(case_reader), LOG_LEVEL_ERROR));

    Mem_Free(data);
    Arena_Free(&arena);
}

TEST(a_case_with_zero_rows_round_trips)
{
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    const uint32_t start_values[] = { 0, 0 };
    const CASE_HEADER header = M_MakeHeader(&arena, "th06", "", start_values);

    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    CASE_WRITER *const case_writer = Case_OpenWriter(&writer, &header);
    CHECK(Result_Log(Case_CloseWriter(case_writer), LOG_LEVEL_ERROR));

    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));

    READER reader;
    Reader_OpenBuffer(&reader, data, size);
    CASE_READER *case_reader;
    CHECK(Result_Log(
        Case_OpenReader(&reader, &arena, &case_reader), LOG_LEVEL_ERROR));
    CHECK(Case_ReadRow(case_reader) == nullptr);
    CHECK(Result_Log(Case_CloseReader(case_reader), LOG_LEVEL_ERROR));

    Mem_Free(data);
    Arena_Free(&arena);
}

TEST(a_long_case_compresses_small)
{
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    const uint32_t start_values[] = { 0, 0 };
    const CASE_HEADER header = M_MakeHeader(&arena, "th06", "", start_values);

    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    CASE_WRITER *const case_writer = Case_OpenWriter(&writer, &header);
    const uint32_t row[2] = { 7, 9 };
    const int32_t row_count = 100000;
    for (int32_t i = 0; i < row_count; i++) {
        Case_WriteRow(case_writer, row);
    }
    CHECK(Result_Log(Case_CloseWriter(case_writer), LOG_LEVEL_ERROR));

    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    CHECK(size < (size_t)row_count * 8 / 10);

    READER reader;
    Reader_OpenBuffer(&reader, data, size);
    CASE_READER *case_reader;
    CHECK(Result_Log(
        Case_OpenReader(&reader, &arena, &case_reader), LOG_LEVEL_ERROR));
    int32_t read_count = 0;
    while (Case_ReadRow(case_reader) != nullptr) {
        read_count++;
    }
    CHECK_EQ_INT(read_count, row_count);
    CHECK(Result_Log(Case_CloseReader(case_reader), LOG_LEVEL_ERROR));

    Mem_Free(data);
    Arena_Free(&arena);
}

TEST(a_bad_magic_fails)
{
    uint8_t bytes[] = "XXXXrest-of-the-bytes-dont-matter";
    READER reader;
    Reader_OpenBuffer(&reader, bytes, sizeof(bytes));
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    CASE_READER *case_reader;
    const RESULT result = Case_OpenReader(&reader, &arena, &case_reader);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "not a case file") != nullptr);
    Result_Ignore(result);
    Arena_Free(&arena);
}

TEST(a_wrong_version_fails)
{
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    const uint32_t start_values[] = { 0, 0 };
    const CASE_HEADER header = M_MakeHeader(&arena, "th06", "", start_values);

    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    CASE_WRITER *const case_writer = Case_OpenWriter(&writer, &header);
    CHECK(Result_Log(Case_CloseWriter(case_writer), LOG_LEVEL_ERROR));
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    data[4] = 2; // the version field, little-endian u16, right after the magic

    READER reader;
    Reader_OpenBuffer(&reader, data, size);
    CASE_READER *case_reader;
    const RESULT result = Case_OpenReader(&reader, &arena, &case_reader);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "case version 2, expected 1") != nullptr);
    Result_Ignore(result);

    Mem_Free(data);
    Arena_Free(&arena);
}

TEST(a_truncated_header_fails)
{
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    const uint32_t start_values[] = { 0, 0 };
    const CASE_HEADER header = M_MakeHeader(&arena, "th06", "", start_values);

    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    CASE_WRITER *const case_writer = Case_OpenWriter(&writer, &header);
    CHECK(Result_Log(Case_CloseWriter(case_writer), LOG_LEVEL_ERROR));
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));

    READER reader;
    Reader_OpenBuffer(&reader, data, 10); // cuts off inside the header
    CASE_READER *case_reader;
    const RESULT result = Case_OpenReader(&reader, &arena, &case_reader);
    CHECK(!IS_OK(result));
    Result_Ignore(result);

    Mem_Free(data);
    Arena_Free(&arena);
}

TEST(a_truncated_stream_fails)
{
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    const uint32_t start_values[] = { 0, 0 };
    const CASE_HEADER header = M_MakeHeader(&arena, "th06", "", start_values);

    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    CASE_WRITER *const case_writer = Case_OpenWriter(&writer, &header);
    const uint32_t row[2] = { 1, 2 };
    Case_WriteRow(case_writer, row);
    Case_WriteRow(case_writer, row);
    CHECK(Result_Log(Case_CloseWriter(case_writer), LOG_LEVEL_ERROR));
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));

    READER reader;
    Reader_OpenBuffer(&reader, data, size - 2); // cuts off inside the stream
    CASE_READER *case_reader;
    CHECK(Result_Log(
        Case_OpenReader(&reader, &arena, &case_reader), LOG_LEVEL_ERROR));
    while (Case_ReadRow(case_reader) != nullptr) {}
    const RESULT result = Case_CloseReader(case_reader);
    CHECK(!IS_OK(result));
    Result_Ignore(result);

    Mem_Free(data);
    Arena_Free(&arena);
}

TEST(trailing_bytes_after_the_stream_fail)
{
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    const uint32_t start_values[] = { 0, 0 };
    const CASE_HEADER header = M_MakeHeader(&arena, "th06", "", start_values);

    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    CASE_WRITER *const case_writer = Case_OpenWriter(&writer, &header);
    const uint32_t row[2] = { 1, 2 };
    Case_WriteRow(case_writer, row);
    CHECK(Result_Log(Case_CloseWriter(case_writer), LOG_LEVEL_ERROR));
    size_t size;
    uint8_t *data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));

    uint8_t *const padded = Mem_Alloc(size + 4);
    memcpy(padded, data, size);
    memset(padded + size, 0xFF, 4);
    Mem_Free(data);
    data = padded;
    size += 4;

    READER reader;
    Reader_OpenBuffer(&reader, data, size);
    CASE_READER *case_reader;
    CHECK(Result_Log(
        Case_OpenReader(&reader, &arena, &case_reader), LOG_LEVEL_ERROR));
    while (Case_ReadRow(case_reader) != nullptr) {}
    const RESULT result = Case_CloseReader(case_reader);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "trailing bytes") != nullptr);
    Result_Ignore(result);

    Mem_Free(data);
    Arena_Free(&arena);
}

// Writes a header with no start columns and the calc names given, field by
// field, so that the test can build headers that Case_OpenWriter refuses.
static void M_WriteRawHeader(
    WRITER *const writer, const char *const *const calc_names,
    const uint32_t calc_count)
{
    Writer_WriteBytes(writer, CASE_MAGIC, 4);
    Writer_WriteU16(writer, CASE_VERSION);
    const uint8_t game_id[CASE_GAME_ID_SIZE] = { 't', 'h', '0', '6' };
    Writer_WriteBytes(writer, game_id, sizeof(game_id));
    Writer_WriteU32(writer, 0);
    Writer_WriteU32(writer, 0);
    Writer_WriteU32(writer, calc_count);
    for (uint32_t i = 0; i < calc_count; i++) {
        Writer_WriteU8(writer, (uint8_t)strlen(calc_names[i]));
        Writer_WriteBytes(writer, calc_names[i], strlen(calc_names[i]));
    }
}

static RESULT M_OpenRawHeader(
    const char *const *const calc_names, const uint32_t calc_count)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    M_WriteRawHeader(&writer, calc_names, calc_count);
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));

    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    READER reader;
    Reader_OpenBuffer(&reader, data, size);
    CASE_READER *case_reader;
    const RESULT result = Case_OpenReader(&reader, &arena, &case_reader);
    if (IS_OK(result)) {
        Result_Ignore(Case_CloseReader(case_reader));
    }
    Mem_Free(data);
    Arena_Free(&arena);
    return result;
}

TEST(a_duplicate_name_fails)
{
    const char *const names[] = { "x", "x" };
    const RESULT result = M_OpenRawHeader(names, ARRAY_SIZE(names));
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "duplicate column name") != nullptr);
    Result_Ignore(result);
}

TEST(a_case_without_calc_columns_fails)
{
    const RESULT result = M_OpenRawHeader(nullptr, 0);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "no calc columns") != nullptr);
    Result_Ignore(result);
}

TEST(reading_after_a_failure_returns_nullptr)
{
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    const uint32_t start_values[] = { 0, 0 };
    const CASE_HEADER header = M_MakeHeader(&arena, "th06", "", start_values);

    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    CASE_WRITER *const case_writer = Case_OpenWriter(&writer, &header);
    const uint32_t row[2] = { 1, 2 };
    Case_WriteRow(case_writer, row);
    CHECK(Result_Log(Case_CloseWriter(case_writer), LOG_LEVEL_ERROR));
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));

    READER reader;
    Reader_OpenBuffer(&reader, data, size - 6);
    CASE_READER *case_reader;
    CHECK(Result_Log(
        Case_OpenReader(&reader, &arena, &case_reader), LOG_LEVEL_ERROR));
    while (Case_ReadRow(case_reader) != nullptr) {}
    CHECK(Case_ReadRow(case_reader) == nullptr);
    const RESULT result = Case_CloseReader(case_reader);
    CHECK(!IS_OK(result));
    CHECK(strstr(result.message, "truncated case stream") != nullptr);
    Result_Ignore(result);

    Mem_Free(data);
    Arena_Free(&arena);
}

TEST(find_column_returns_the_index_or_minus_one)
{
    ARENA arena;
    Arena_Init(&arena, "test", M_ARENA_SIZE);
    CASE_NAMES names;
    Array_Init(&names, Arena_Allocator(&arena));
    Array_Append(&names, "x");
    Array_Append(&names, "y");
    CHECK_EQ_INT(Case_FindColumn(&names, "x"), 0);
    CHECK_EQ_INT(Case_FindColumn(&names, "y"), 1);
    CHECK_EQ_INT(Case_FindColumn(&names, "z"), -1);
    Arena_Free(&arena);
}

// A byte-wise FNV-1a reference, folding a value's bytes in little-endian
// order, to check Case_Hash against an implementation that does not share
// its code.
static uint32_t M_ReferenceHash(uint32_t hash, const uint32_t value)
{
    const uint8_t bytes[4] = {
        (uint8_t)value,
        (uint8_t)(value >> 8),
        (uint8_t)(value >> 16),
        (uint8_t)(value >> 24),
    };
    for (int i = 0; i < 4; i++) {
        hash = (hash ^ bytes[i]) * 16777619u;
    }
    return hash;
}

TEST(hash_matches_a_byte_wise_fnv1a_reference)
{
    uint32_t hash = CASE_HASH_INIT;
    uint32_t reference = CASE_HASH_INIT;
    const uint32_t values[] = { 0, 1, 0xFFFFFFFFu, 0x12345678u, 0x80000000u };
    for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
        hash = Case_Hash(hash, values[i]);
        reference = M_ReferenceHash(reference, values[i]);
    }
    CHECK_EQ_INT(hash, reference);
}
