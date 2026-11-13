// SPDX-License-Identifier: GPL-3.0-only

#include "core/io/reader.h"

#include "test.h"
#include "test_allocator.h"
#include "test_dir.h"

#define M_FILE_SIZE (3 * READER_BUFFER_SIZE + 123)

static uint8_t m_Pattern[M_FILE_SIZE];

static void M_MakePattern(void)
{
    for (size_t i = 0; i < M_FILE_SIZE; i++) {
        m_Pattern[i] = (uint8_t)(i * 7 + (i >> 8));
    }
}

static uint64_t M_PatternAt(const size_t offset, const size_t size)
{
    uint64_t value = 0;
    for (size_t i = 0; i < size; i++) {
        value |= (uint64_t)m_Pattern[offset + i] << (8 * i);
    }
    return value;
}

static void M_Create(
    const char *const path, const void *const data, const size_t size)
{
    if (!SDL_SaveFile(path, data, size)) {
        TEST_FAIL("cannot create %s: %s", path, SDL_GetError());
    }
}

// Returns the path of a file of M_FILE_SIZE bytes of the pattern, in the
// directory of the test case name.
static char *M_PrepareFile(const char *const name)
{
    M_MakePattern();
    char *const directory = TestDir_Prepare(name);
    char *const path = TestDir_Join(directory, "pattern.dat");
    M_Create(path, m_Pattern, sizeof(m_Pattern));
    Mem_Free(directory);
    return path;
}

// Checks that a reader failed with message followed by " in <path>", or,
// when that is too long for the reader's buffer, by " in ..." and the end
// of path with its file name whole.
static bool M_FailsIn(
    const READER *const reader, const char *const message,
    const char *const path)
{
    const RESULT result = Reader_Result(reader);
    if (IS_OK(result)) {
        return false;
    }
    char *const whole = Str_Format("%s in %s", message, path);
    char *const elided = Str_Format("%s in ...", message);
    const char *const name = strrchr(path, '/');
    const size_t length = strlen(result.message);
    bool matches = strcmp(result.message, whole) == 0;
    if (!matches && strlen(whole) >= READER_ERROR_SIZE) {
        matches = Str_StartsWith(result.message, elided)
            && Str_EndsWith(result.message, name != nullptr ? name : path)
            && length < READER_ERROR_SIZE;
    }
    if (!matches) {
        printf("    message: %s\n", result.message);
    }
    Mem_Free(elided);
    Mem_Free(whole);
    Result_Ignore(result);
    return matches;
}

TEST(every_byte_reads_in_order)
{
    char *const path = M_PrepareFile("in_order");
    READER reader;
    CHECK(Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Reader_Size(&reader), M_FILE_SIZE);
    bool matches = true;
    for (size_t i = 0; i < M_FILE_SIZE; i++) {
        matches = matches && Reader_ReadU8(&reader) == m_Pattern[i];
    }
    CHECK(matches);
    CHECK_EQ_INT(Reader_Remaining(&reader), 0);
    CHECK(Result_Ignore(Reader_Result(&reader)));
    Reader_Close(&reader);
    Mem_Free(path);
}

TEST(values_across_block_boundaries_read_whole)
{
    char *const path = M_PrepareFile("boundaries");
    READER reader;
    CHECK(Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR));
    for (size_t block = 1; block <= 3; block++) {
        const size_t boundary = block * READER_BUFFER_SIZE;
        for (size_t before = 1; before < 8; before++) {
            Reader_Seek(&reader, boundary - before);
            uint64_t value;
            Reader_Read(&reader, &value);
            if (value != M_PatternAt(boundary - before, 8)) {
                TEST_FAIL("u64 at %zu", boundary - before);
            }
        }
        Reader_Seek(&reader, boundary - 2);
        CHECK(Reader_ReadU32(&reader) == M_PatternAt(boundary - 2, 4));
    }
    CHECK(!Reader_Failed(&reader));
    Reader_Close(&reader);
    Mem_Free(path);
}

TEST(seeking_back_and_forth_rereads_the_file)
{
    char *const path = M_PrepareFile("seek");
    READER reader;
    CHECK(Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR));
    const size_t offsets[] = {
        M_FILE_SIZE - 8,        0, READER_BUFFER_SIZE + 5, 17, M_FILE_SIZE - 8,
        2 * READER_BUFFER_SIZE,
    };
    for (size_t i = 0; i < ARRAY_SIZE(offsets); i++) {
        Reader_Seek(&reader, offsets[i]);
        CHECK(Reader_ReadU64(&reader) == M_PatternAt(offsets[i], 8));
        CHECK_EQ_INT(Reader_Tell(&reader), offsets[i] + 8);
    }
    Reader_Skip(&reader, READER_BUFFER_SIZE);
    CHECK(
        Reader_ReadU16(&reader) == M_PatternAt(3 * READER_BUFFER_SIZE + 8, 2));
    CHECK(!Reader_Failed(&reader));
    Reader_Close(&reader);
    Mem_Free(path);
}

TEST(large_reads_copy_every_byte)
{
    char *const path = M_PrepareFile("large");
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    READER reader;
    CHECK(Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR));
    Reader_Skip(&reader, 5);
    CHECK_EQ_INT(Reader_ReadU8(&reader), m_Pattern[5]);
    const size_t size = 2 * READER_BUFFER_SIZE + 10;
    uint8_t *const block = Reader_ReadAlloc(&reader, size, &test.allocator);
    CHECK(block != nullptr && memcmp(block, m_Pattern + 6, size) == 0);
    Allocator_Free(&test.allocator, block, size);

    uint8_t small[16];
    Reader_Seek(&reader, READER_BUFFER_SIZE - 8);
    Reader_ReadBytes(&reader, small, sizeof(small));
    CHECK(memcmp(small, m_Pattern + READER_BUFFER_SIZE - 8, 16) == 0);
    Reader_Seek(&reader, 0);
    uint8_t *const whole =
        Reader_ReadAlloc(&reader, M_FILE_SIZE, &test.allocator);
    CHECK(whole != nullptr && memcmp(whole, m_Pattern, M_FILE_SIZE) == 0);
    Allocator_Free(&test.allocator, whole, M_FILE_SIZE);
    CHECK(!Reader_Failed(&reader));
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.size_errors, 0);
    Reader_Close(&reader);
    Mem_Free(path);
}

TEST(slices_read_through_the_file)
{
    char *const path = M_PrepareFile("slices");
    READER reader;
    CHECK(Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR));
    const size_t start = READER_BUFFER_SIZE - 10;
    Reader_Seek(&reader, start);
    READER slice = Reader_Slice(&reader, 2 * READER_BUFFER_SIZE);
    CHECK_EQ_INT(Reader_Tell(&reader), start + 2 * READER_BUFFER_SIZE);
    CHECK_EQ_INT(Reader_Size(&slice), 2 * READER_BUFFER_SIZE);

    bool matches = true;
    for (size_t i = 0; i < 2 * READER_BUFFER_SIZE; i += 4) {
        matches =
            matches && Reader_ReadU32(&slice) == M_PatternAt(start + i, 4);
        if (i % 4096 == 0) {
            Reader_Seek(&reader, i);
            matches = matches && Reader_ReadU8(&reader) == m_Pattern[i];
        }
    }
    CHECK(matches);

    Reader_Seek(&slice, 100);
    READER inner = Reader_Slice(&slice, 8);
    CHECK(Reader_ReadU64(&inner) == M_PatternAt(start + 100, 8));
    CHECK(!Reader_Failed(&reader) && !Reader_Failed(&slice));

    Reader_ReadU8(&inner);
    char *const expected = Str_Format(
        "read of 1 bytes at 0x%zx past end 0x%zx", start + 108, start + 108);
    CHECK(M_FailsIn(&inner, expected, path));
    CHECK(!Reader_Failed(&slice));
    Reader_Close(&slice);
    Reader_Seek(&reader, 0);
    CHECK_EQ_INT(Reader_ReadU8(&reader), m_Pattern[0]);
    Mem_Free(expected);
    Reader_Close(&reader);
    Mem_Free(path);
}

TEST(reading_past_the_end_names_the_path)
{
    char *const directory = TestDir_Prepare("past_end");
    char *const path = TestDir_Join(directory, "short.dat");
    M_Create(path, "abc", 3);
    READER reader;
    CHECK(Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Reader_ReadU32(&reader), 0);
    const char *const expected = "read of 4 bytes at 0x0 past end 0x3";
    CHECK(M_FailsIn(&reader, expected, path));

    Reader_Close(&reader);
    CHECK(M_FailsIn(&reader, expected, path));
    CHECK_EQ_INT(Reader_Size(&reader), 0);
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(result_still_names_the_path_after_close)
{
    char *const directory = TestDir_Prepare("after_close");
    char *const path = TestDir_Join(directory, "short.dat");
    M_Create(path, "abc", 3);
    READER reader;
    CHECK(Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Reader_ReadU32(&reader), 0);
    Reader_Close(&reader);
    CHECK(M_FailsIn(&reader, "read of 4 bytes at 0x0 past end 0x3", path));
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(a_long_path_is_cut_at_the_start)
{
    char *directory = TestDir_Prepare("long_path");
    for (int i = 0; i < 6; i++) {
        char *const deeper =
            TestDir_Join(directory, "a_directory_with_a_long_name");
        CHECK(SDL_CreateDirectory(deeper));
        Mem_Free(directory);
        directory = deeper;
    }
    char *const path = TestDir_Join(directory, "紅魔郷CM.DAT");
    M_Create(path, "abc", 3);
    READER reader;
    CHECK(Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR));
    Reader_ReadU32(&reader);
    Reader_Close(&reader);

    const RESULT result = Reader_Result(&reader);
    CHECK(!IS_OK(result));
    CHECK(Str_StartsWith(
        result.message, "read of 4 bytes at 0x0 past end 0x3 in .../"));
    CHECK(Str_EndsWith(result.message, "/紅魔郷CM.DAT"));
    CHECK(strlen(result.message) < READER_ERROR_SIZE);
    Result_Ignore(result);
    Mem_Free(path);
    Mem_Free(directory);
}

// Windows does not let a file that a READER has open shrink, so this
// test runs elsewhere only.
#ifndef _WIN32
TEST(a_file_that_shrinks_fails_with_the_reason)
{
    char *const path = M_PrepareFile("shrinks");
    READER reader;
    CHECK(Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR));
    M_Create(path, "abc", 3);
    Reader_Skip(&reader, 2);
    CHECK_EQ_INT(Reader_ReadU32(&reader), 0);
    CHECK_EQ_INT(Reader_Tell(&reader), 2);
    const char *const expected =
        "read of 4 bytes at 0x2 failed (the file is shorter than when it was "
        "opened)";
    CHECK(M_FailsIn(&reader, expected, path));
    Reader_Close(&reader);
    CHECK(M_FailsIn(&reader, expected, path));
    Mem_Free(path);
}
#endif

TEST(an_empty_file_has_nothing_to_read)
{
    char *const directory = TestDir_Prepare("empty");
    char *const path = TestDir_Join(directory, "empty.dat");
    M_Create(path, "", 0);
    READER reader;
    CHECK(Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Reader_Size(&reader), 0);
    CHECK_EQ_INT(Reader_ReadU8(&reader), 0);
    CHECK(Reader_Failed(&reader));
    Reader_Close(&reader);
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(opening_a_missing_file_fails)
{
    char *const directory = TestDir_Prepare("missing");
    char *const path = TestDir_Join(directory, "missing.dat");
    READER reader;
    const RESULT result = Reader_OpenFile(&reader, path);
    char *const prefix = Str_Format("Cannot open %s: ", path);
    CHECK(!IS_OK(result) && Str_StartsWith(result.message, prefix));
    Result_Ignore(result);
    CHECK_EQ_INT(Reader_Size(&reader), 0);
    CHECK(!Reader_Failed(&reader));
    Reader_Close(&reader);
    Mem_Free(prefix);
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(opening_a_directory_fails)
{
    char *const directory = TestDir_Prepare("directory");
    READER reader;
    const RESULT result = Reader_OpenFile(&reader, directory);
    char *const prefix = Str_Format("Cannot open %s: ", directory);
    CHECK(!IS_OK(result) && Str_StartsWith(result.message, prefix));
    Result_Ignore(result);
    Reader_Close(&reader);
    Mem_Free(prefix);
    Mem_Free(directory);
}
