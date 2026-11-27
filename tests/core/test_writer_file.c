// Declares symlink, which the tests use to make a file that cannot be
// written to.
#define _POSIX_C_SOURCE 200809L

#include "core/io/reader.h"
#include "core/io/writer.h"

#include "test.h"
#include "test_dir.h"

#include <unistd.h>

#define M_BODY_SIZE 200000

static bool M_Contains(
    const char *const path, const void *const bytes, const size_t size)
{
    size_t actual_size;
    void *const actual = SDL_LoadFile(path, &actual_size);
    const bool matches = actual != nullptr && actual_size == size
        && memcmp(actual, bytes, size) == 0;
    SDL_free(actual);
    return matches;
}

static void M_Create(const char *const path, const char *const text)
{
    if (!SDL_SaveFile(path, text, strlen(text))) {
        TEST_FAIL("cannot create %s: %s", path, SDL_GetError());
    }
}

static bool M_Exists(const char *const path)
{
    return SDL_GetPathInfo(path, nullptr);
}

static SDL_EnumerationResult SDLCALL M_CountEntry(
    void *const userdata, const char *const directory, const char *const name)
{
    (*(int *)userdata)++;
    return SDL_ENUM_CONTINUE;
}

static int M_CountEntries(const char *const directory)
{
    int count = 0;
    SDL_EnumerateDirectory(directory, M_CountEntry, &count);
    return count;
}

static bool M_FailsWith(const RESULT result, const char *const prefix)
{
    const bool matches =
        !IS_OK(result) && Str_StartsWith(result.message, prefix);
    if (!IS_OK(result) && !matches) {
        printf("    message: %s\n", result.message);
    }
    Result_Ignore(result);
    return matches;
}

TEST(close_puts_the_file_in_place)
{
    char *const directory = TestDir_Prepare("in_place");
    char *const path = TestDir_Join(directory, "score.dat");
    char *const temp_path = Str_Format("%s.tmp", path);
    WRITER writer;
    CHECK(Result_Log(Writer_OpenFile(&writer, path), LOG_LEVEL_ERROR));
    const uint32_t magic = 0x36304854;
    Writer_Write(&writer, &magic);
    Writer_WriteU16(&writer, 0x0102);
    CHECK(!M_Exists(path));
    CHECK(M_Exists(temp_path));
    CHECK(!Writer_Failed(&writer));
    CHECK(Result_Ignore(Writer_Result(&writer)));

    CHECK(Result_Log(Writer_Close(&writer), LOG_LEVEL_ERROR));
    CHECK(M_Contains(path, "TH06\x02\x01", 6));
    CHECK(!M_Exists(temp_path));
    CHECK_EQ_INT(M_CountEntries(directory), 1);
    Mem_Free(temp_path);
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(a_file_writer_is_named_by_its_target_path)
{
    char *const directory = TestDir_Prepare("name");
    char *const path = TestDir_Join(directory, "score.dat");
    WRITER writer;
    CHECK(Result_Log(Writer_OpenFile(&writer, path), LOG_LEVEL_ERROR));
    CHECK_EQ_STR(Writer_GetName(&writer), path);
    CHECK(Result_Log(Writer_Close(&writer), LOG_LEVEL_ERROR));
    CHECK(Writer_GetName(&writer) == nullptr);
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(close_replaces_an_existing_file)
{
    char *const directory = TestDir_Prepare("replace");
    char *const path = TestDir_Join(directory, "score.dat");
    M_Create(path, "a longer old content");
    WRITER writer;
    CHECK(Result_Log(Writer_OpenFile(&writer, path), LOG_LEVEL_ERROR));
    Writer_WriteBytes(&writer, "new", 3);
    CHECK(M_Contains(path, "a longer old content", 20));
    CHECK(Result_Log(Writer_Close(&writer), LOG_LEVEL_ERROR));
    CHECK(M_Contains(path, "new", 3));
    CHECK_EQ_INT(M_CountEntries(directory), 1);
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(an_empty_file_is_written)
{
    char *const directory = TestDir_Prepare("empty");
    char *const path = TestDir_Join(directory, "empty.dat");
    WRITER writer;
    CHECK(Result_Log(Writer_OpenFile(&writer, path), LOG_LEVEL_ERROR));
    CHECK(Result_Log(Writer_Close(&writer), LOG_LEVEL_ERROR));
    CHECK(M_Contains(path, "", 0));
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(seeking_back_patches_across_blocks)
{
    char *const directory = TestDir_Prepare("patch");
    char *const path = TestDir_Join(directory, "replay.rpy");
    WRITER writer;
    CHECK(Result_Log(Writer_OpenFile(&writer, path), LOG_LEVEL_ERROR));
    Writer_WriteU32(&writer, 0);
    for (uint32_t i = 0; i < M_BODY_SIZE / 4; i++) {
        Writer_WriteU32(&writer, i);
    }
    const size_t end = Writer_Tell(&writer);
    Writer_Seek(&writer, 0);
    Writer_WriteU32(&writer, (uint32_t)(end - 4));
    Writer_Seek(&writer, 4 + 4 * 20000);
    Writer_WriteU32(&writer, 0xDEADBEEF);
    Writer_Seek(&writer, end - 2);
    Writer_WriteU32(&writer, 0xCAFEF00D);
    CHECK_EQ_INT(Writer_Tell(&writer), end + 2);
    CHECK(Result_Log(Writer_Close(&writer), LOG_LEVEL_ERROR));

    READER reader;
    CHECK(Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Reader_Size(&reader), end + 2);
    CHECK_EQ_INT(Reader_ReadU32(&reader), end - 4);
    bool matches = true;
    for (uint32_t i = 0; i < M_BODY_SIZE / 4 - 1; i++) {
        const uint32_t expected = i == 20000 ? 0xDEADBEEF : i;
        matches = matches && Reader_ReadU32(&reader) == expected;
    }
    CHECK(matches);
    CHECK_EQ_INT(Reader_ReadU16(&reader), (M_BODY_SIZE / 4 - 1) & 0xFFFF);
    CHECK_EQ_INT(Reader_ReadU32(&reader), 0xCAFEF00D);
    CHECK(Result_Log(Reader_Result(&reader), LOG_LEVEL_ERROR));
    Reader_Close(&reader);
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(opening_in_a_missing_directory_fails)
{
    char *const directory = TestDir_Prepare("missing");
    char *const path = TestDir_Join(directory, "missing/score.dat");
    char *const prefix = Str_Format("Cannot create %s.tmp: ", path);
    WRITER writer;
    CHECK(M_FailsWith(Writer_OpenFile(&writer, path), prefix));
    CHECK(Result_Ignore(Writer_Close(&writer)));
    CHECK_EQ_INT(M_CountEntries(directory), 0);
    Mem_Free(prefix);
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(a_failed_close_leaves_the_target_and_no_temporary)
{
    char *const directory = TestDir_Prepare("failed_close");
    char *const path = TestDir_Join(directory, "occupied");
    char *const inner = TestDir_Join(path, "file");
    char *const temp_path = Str_Format("%s.tmp", path);
    SDL_CreateDirectory(path);
    M_Create(inner, "x");

    WRITER writer;
    CHECK(Result_Log(Writer_OpenFile(&writer, path), LOG_LEVEL_ERROR));
    Writer_WriteBytes(&writer, "abc", 3);
    char *const prefix = Str_Format("Cannot replace %s: ", path);
    CHECK(M_FailsWith(Writer_Close(&writer), prefix));
    CHECK(!M_Exists(temp_path));
    CHECK(M_Contains(inner, "x", 1));
    CHECK_EQ_INT(M_CountEntries(directory), 1);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    Mem_Free(prefix);
    Mem_Free(temp_path);
    Mem_Free(inner);
    Mem_Free(path);
    Mem_Free(directory);
}

// /dev/full exists only on Linux.
#ifdef __linux__
TEST(a_failed_write_is_sticky_and_leaves_the_target)
{
    // The temporary file is a link to /dev/full, which fails every write
    // with "No space left on device".
    char *const directory = TestDir_Prepare("full");
    char *const path = TestDir_Join(directory, "score.dat");
    char *const temp_path = Str_Format("%s.tmp", path);
    M_Create(path, "old");
    if (symlink("/dev/full", temp_path) != 0) {
        TEST_FAIL("cannot link %s to /dev/full", temp_path);
    }

    WRITER writer;
    CHECK(Result_Log(Writer_OpenFile(&writer, path), LOG_LEVEL_ERROR));
    for (uint32_t i = 0; i < M_BODY_SIZE / 4; i++) {
        Writer_WriteU32(&writer, i);
    }
    CHECK(Writer_Failed(&writer));
    const size_t position = Writer_Tell(&writer);
    Writer_WriteU32(&writer, 0);
    CHECK_EQ_INT(Writer_Tell(&writer), position);
    char *const prefix = Str_Format("Cannot write %s: ", path);
    CHECK(M_FailsWith(Writer_Result(&writer), prefix));
    CHECK(M_FailsWith(Writer_Close(&writer), prefix));
    CHECK(!M_Exists(temp_path));
    CHECK(M_Contains(path, "old", 3));
    Mem_Free(prefix);
    Mem_Free(temp_path);
    Mem_Free(path);
    Mem_Free(directory);
}
#endif
