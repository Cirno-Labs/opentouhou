#include "tests/test.h"
#include "tests/test_dir.h"
#include "th02/high_scores/file.h"

#include "core/base/memory.h"
#include "core/io/reader.h"
#include "core/io/writer.h"

#include <string.h>

typedef struct {
    VFS *original;
    VFS *user;
    char *original_dir;
    char *user_dir;
} FIXTURE;

static FIXTURE M_Open(const bool writable)
{
    FIXTURE fixture = {
        .original = Vfs_Create(&g_HeapAllocator),
        .user = Vfs_Create(&g_HeapAllocator),
        .original_dir = TestDir_Prepare("original"),
        .user_dir = TestDir_Prepare("user"),
    };
    CHECK(Result_Ignore(
        Vfs_MountDir(fixture.original, "", fixture.original_dir, 0, false)));
    CHECK(Result_Ignore(
        Vfs_MountDir(fixture.user, "", fixture.user_dir, 0, writable)));
    return fixture;
}

static void M_Close(FIXTURE *const fixture)
{
    Vfs_Destroy(fixture->user);
    Vfs_Destroy(fixture->original);
    Mem_Free(fixture->user_dir);
    Mem_Free(fixture->original_dir);
}

static void M_Write(
    const char *const directory, const char *const name, const void *const data,
    const size_t size)
{
    char *const path = TestDir_Join(directory, name);
    TestDir_Write(path, data, size);
    Mem_Free(path);
}

static void M_Original(FIXTURE *const fixture, const HIGH_SCORES *const scores)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    CHECK(Result_Ignore(HighScores_Write(&writer, scores)));
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    CHECK(Result_Ignore(Writer_Close(&writer)));
    M_Write(fixture->original_dir, "HUUHI.DAT", bytes, size);
    Mem_Free(bytes);
    Vfs_Destroy(fixture->original);
    fixture->original = Vfs_Create(&g_HeapAllocator);
    CHECK(Result_Ignore(
        Vfs_MountDir(fixture->original, "", fixture->original_dir, 0, false)));
}

static void M_CheckDisk(
    const char *const directory, const HIGH_SCORES *const expected)
{
    char *const path = TestDir_Join(directory, "HUUHI.DAT");
    READER reader;
    CHECK(Result_Ignore(Reader_OpenFile(&reader, path)));
    CHECK_EQ_INT(
        Reader_Size(&reader), HIGH_SCORES_SECTIONS * HIGH_SCORES_SECTION_SIZE);
    HIGH_SCORES actual;
    CHECK(Result_Ignore(HighScores_Read(&reader, &actual)));
    CHECK(memcmp(&actual, expected, sizeof(actual)) == 0);
    Reader_Close(&reader);
    Mem_Free(path);
}

TEST(missing_file_creates_five_factory_tables_only_in_user_storage)
{
    FIXTURE fixture = M_Open(true);
    HIGH_SCORES expected;
    HIGH_SCORES actual;
    HighScores_Defaults(&expected);
    CHECK(Result_Ignore(
        HighScoresFile_Load(fixture.original, fixture.user, &actual)));
    CHECK(memcmp(&actual, &expected, sizeof(actual)) == 0);
    CHECK(!Vfs_Exists(fixture.original, "HUUHI.DAT"));
    M_CheckDisk(fixture.user_dir, &expected);
    M_Close(&fixture);
}

TEST(first_launch_imports_original_but_existing_user_scores_take_priority)
{
    FIXTURE fixture = M_Open(true);
    HIGH_SCORES original;
    HIGH_SCORES user;
    HIGH_SCORES actual;
    HighScores_Defaults(&original);
    original.sections[0].cleared = 318;
    original.sections[4].records[9].score = 9876543;
    M_Original(&fixture, &original);
    CHECK(Result_Ignore(
        HighScoresFile_Load(fixture.original, fixture.user, &actual)));
    CHECK(memcmp(&actual, &original, sizeof(actual)) == 0);
    M_CheckDisk(fixture.user_dir, &original);
    user = original;
    user.sections[0].cleared = 0;
    user.sections[1].records[0].score = 999999;
    CHECK(Result_Ignore(HighScoresFile_Save(fixture.user, &user)));
    CHECK(Result_Ignore(
        HighScoresFile_Load(fixture.original, fixture.user, &actual)));
    CHECK(memcmp(&actual, &user, sizeof(actual)) == 0);
    M_CheckDisk(fixture.user_dir, &user);
    M_CheckDisk(fixture.original_dir, &original);
    M_Close(&fixture);
}

TEST(native_configuration_prevents_reimport_after_a_user_file_was_removed)
{
    FIXTURE fixture = M_Open(true);
    HIGH_SCORES original;
    HIGH_SCORES expected;
    HIGH_SCORES actual;
    HighScores_Defaults(&original);
    original.sections[0].cleared = 318;
    original.sections[1].cleared = 118;
    original.sections[2].cleared = 218;
    M_Original(&fixture, &original);
    M_Write(fixture.user_dir, "CONFIG.INI", "", 0);
    HighScores_Defaults(&expected);
    CHECK(Result_Ignore(
        HighScoresFile_Load(fixture.original, fixture.user, &actual)));
    CHECK(memcmp(&actual, &expected, sizeof(actual)) == 0);
    M_CheckDisk(fixture.user_dir, &expected);
    M_CheckDisk(fixture.original_dir, &original);
    M_Close(&fixture);
}

TEST(corrupt_user_file_is_removed_and_only_the_next_launch_creates_defaults)
{
    FIXTURE fixture = M_Open(true);
    HIGH_SCORES original;
    HIGH_SCORES actual;
    HIGH_SCORES expected;
    HighScores_Defaults(&original);
    original.sections[0].cleared = 318;
    M_Original(&fixture, &original);
    M_Write(fixture.user_dir, "HuuHi.Dat", "corrupt", 7);
    M_Write(fixture.user_dir, "config.ini", "", 0);
    memset(&actual, 0xa5, sizeof(actual));
    const HIGH_SCORES before = actual;
    CHECK(!Result_Ignore(
        HighScoresFile_Load(fixture.original, fixture.user, &actual)));
    CHECK(memcmp(&actual, &before, sizeof(actual)) == 0);
    CHECK(!Vfs_Exists(fixture.user, "HUUHI.DAT"));
    M_CheckDisk(fixture.original_dir, &original);
    CHECK(Result_Ignore(
        HighScoresFile_Load(fixture.original, fixture.user, &actual)));
    HighScores_Defaults(&expected);
    CHECK(memcmp(&actual, &expected, sizeof(actual)) == 0);
    M_CheckDisk(fixture.user_dir, &expected);
    M_Close(&fixture);
}

TEST(corrupt_original_import_is_never_removed_or_replaced)
{
    FIXTURE fixture = M_Open(true);
    M_Write(fixture.original_dir, "HUUHI.DAT", "corrupt", 7);
    Vfs_Destroy(fixture.original);
    fixture.original = Vfs_Create(&g_HeapAllocator);
    CHECK(Result_Ignore(
        Vfs_MountDir(fixture.original, "", fixture.original_dir, 0, false)));
    HIGH_SCORES actual = {};
    const HIGH_SCORES before = actual;
    CHECK(!Result_Ignore(
        HighScoresFile_Load(fixture.original, fixture.user, &actual)));
    CHECK(memcmp(&actual, &before, sizeof(actual)) == 0);
    CHECK(!Vfs_Exists(fixture.user, "HUUHI.DAT"));
    READER reader;
    CHECK(Result_Ignore(
        Vfs_Open(fixture.original, "HUUHI.DAT", VFS_STREAM, &reader)));
    char bytes[7];
    Reader_ReadBytes(&reader, bytes, sizeof(bytes));
    CHECK(Result_Ignore(Reader_Result(&reader)));
    CHECK_EQ_INT(Reader_Size(&reader), sizeof(bytes));
    CHECK(memcmp(bytes, "corrupt", sizeof(bytes)) == 0);
    Reader_Close(&reader);
    M_Close(&fixture);
}

TEST(write_and_removal_failures_preserve_output_and_existing_data)
{
    FIXTURE fixture = M_Open(false);
    HIGH_SCORES actual = {};
    const HIGH_SCORES before = actual;
    CHECK(!Result_Ignore(
        HighScoresFile_Load(fixture.original, fixture.user, &actual)));
    CHECK(memcmp(&actual, &before, sizeof(actual)) == 0);
    CHECK(!Vfs_Exists(fixture.user, "HUUHI.DAT"));
    M_Write(fixture.user_dir, "HUUHI.DAT", "corrupt", 7);
    Vfs_Destroy(fixture.user);
    fixture.user = Vfs_Create(&g_HeapAllocator);
    CHECK(Result_Ignore(
        Vfs_MountDir(fixture.user, "", fixture.user_dir, 0, false)));
    CHECK(!Result_Ignore(
        HighScoresFile_Load(fixture.original, fixture.user, &actual)));
    CHECK(memcmp(&actual, &before, sizeof(actual)) == 0);
    CHECK(Vfs_Exists(fixture.user, "HUUHI.DAT"));
    M_Close(&fixture);
}

TEST(valid_user_file_is_accepted_without_any_write)
{
    FIXTURE fixture = M_Open(true);
    HIGH_SCORES expected;
    HIGH_SCORES actual;
    HighScores_Defaults(&expected);
    expected.sections[4].records[0].score = 2345678;
    CHECK(Result_Ignore(HighScoresFile_Save(fixture.user, &expected)));
    Vfs_Destroy(fixture.user);
    fixture.user = Vfs_Create(&g_HeapAllocator);
    CHECK(Result_Ignore(
        Vfs_MountDir(fixture.user, "", fixture.user_dir, 0, false)));
    CHECK(Result_Ignore(
        HighScoresFile_Load(fixture.original, fixture.user, &actual)));
    CHECK(memcmp(&actual, &expected, sizeof(actual)) == 0);
    M_CheckDisk(fixture.user_dir, &expected);
    M_Close(&fixture);
}
