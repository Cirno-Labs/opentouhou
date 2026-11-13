// SPDX-License-Identifier: GPL-3.0-only

#include "core/vfs.h"

#include "core/base/string.h"
#include "core/vfs/dir_source.h"
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

typedef struct {
    char *paths[2];
    int32_t count;
} VISITED_PATHS;

static void M_Visit(void *const user, const char *const path)
{
    VISITED_PATHS *const visited = (VISITED_PATHS *)user;
    visited->paths[visited->count++] = Str_Dup(path);
}

TEST(listing_visits_the_recorded_paths_in_order)
{
    char *const directory = TestDir_Prepare("listed");
    char *const first_file = TestDir_Join(directory, "Bgm.wav");
    char *const second_file = TestDir_Join(directory, "Title00.jpg");
    M_Create(first_file, "a song");
    M_Create(second_file, "a title");

    VFS_SOURCE *source;
    CHECK(Result_Log(
        DirSource_Create(directory, &g_HeapAllocator, &source),
        LOG_LEVEL_ERROR));

    VISITED_PATHS visited = { .count = 0 };
    source->funcs->list(source, M_Visit, &visited);
    CHECK_EQ_INT(visited.count, 2);
    CHECK(strcmp(visited.paths[0], "bgm.wav") == 0);
    CHECK(strcmp(visited.paths[1], "title00.jpg") == 0);
    Mem_Free(visited.paths[0]);
    Mem_Free(visited.paths[1]);

    source->funcs->close(source);
    Mem_Free(second_file);
    Mem_Free(first_file);
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
