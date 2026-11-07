#include "core/vfs.h"

#include "test.h"
#include "test_allocator.h"
#include "test_dir.h"

#include <string.h>

static void M_Create(const char *const path, const char *const content)
{
    if (!SDL_SaveFile(path, content, strlen(content))) {
        TEST_FAIL("cannot create %s: %s", path, SDL_GetError());
    }
}

static bool M_ReadsAs(READER *const reader, const char *const expected)
{
    const size_t size = Reader_Size(reader);
    char *const buffer = Mem_Alloc(size + 1);
    Reader_ReadBytes(reader, buffer, size);
    buffer[size] = '\0';
    const bool matches = strcmp(buffer, expected) == 0;
    Mem_Free(buffer);
    return matches && Result_Ignore(Reader_Result(reader));
}

TEST(nested_mixed_case_files_open_in_both_modes)
{
    char *const directory = TestDir_Prepare("nested");
    char *const sub = TestDir_Join(directory, "BGM");
    SDL_CreateDirectory(sub);
    char *const top_file = TestDir_Join(directory, "Title00.jpg");
    char *const nested_file = TestDir_Join(sub, "TH06_01.wav");
    M_Create(top_file, "a title");
    M_Create(nested_file, "a song");

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    CHECK(Result_Log(Vfs_MountDir(vfs, directory, 0), LOG_LEVEL_ERROR));

    CHECK(Vfs_Exists(vfs, "title00.jpg"));
    CHECK(Vfs_Exists(vfs, "bgm/th06_01.wav"));

    READER reader;
    CHECK(Result_Log(
        Vfs_Open(vfs, "title00.jpg", VFS_IN_MEMORY, &reader), LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "a title"));
    Reader_Close(&reader);

    CHECK(Result_Log(
        Vfs_Open(vfs, "BGM/TH06_01.WAV", VFS_STREAM, &reader),
        LOG_LEVEL_ERROR));
    CHECK(M_ReadsAs(&reader, "a song"));
    Reader_Close(&reader);

    Vfs_Destroy(vfs);
    CHECK_EQ_INT(test.live_blocks, 0);
    Mem_Free(nested_file);
    Mem_Free(top_file);
    Mem_Free(sub);
    Mem_Free(directory);
}

TEST(a_missing_directory_fails_the_mount)
{
    char *const directory = TestDir_Prepare("missing");
    char *const missing = TestDir_Join(directory, "does_not_exist");

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    const RESULT result = Vfs_MountDir(vfs, missing, 0);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    CHECK(!Vfs_Exists(vfs, "anything"));

    Vfs_Destroy(vfs);
    CHECK_EQ_INT(test.live_blocks, 0);
    Mem_Free(missing);
    Mem_Free(directory);
}
