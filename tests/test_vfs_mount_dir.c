#include "core/vfs.h"

#include "core/base/string.h"
#include "core/base/utils.h"
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

TEST(nested_mixed_case_files_open_in_both_modes_named_by_path)
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
    CHECK(Result_Log(
        Vfs_MountDir(vfs, "", directory, 0, false), LOG_LEVEL_ERROR));

    CHECK(Vfs_Exists(vfs, "title00.jpg"));
    CHECK(Vfs_Exists(vfs, "bgm/th06_01.wav"));

    READER reader;
    CHECK(Result_Log(
        Vfs_Open(vfs, "title00.jpg", VFS_IN_MEMORY, &reader), LOG_LEVEL_ERROR));
    CHECK_EQ_STR(Reader_GetName(&reader), top_file);
    CHECK(M_ReadsAs(&reader, "a title"));
    Reader_Close(&reader);

    CHECK(Result_Log(
        Vfs_Open(vfs, "BGM/TH06_01.WAV", VFS_STREAM, &reader),
        LOG_LEVEL_ERROR));
    CHECK_EQ_STR(Reader_GetName(&reader), nested_file);
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

TEST(listing_visits_the_paths_as_the_disk_spells_them)
{
    char *const directory = TestDir_Prepare("listed");
    char *const first_file = TestDir_Join(directory, "Bgm.wav");
    char *const second_file = TestDir_Join(directory, "Title00.jpg");
    M_Create(first_file, "a song");
    M_Create(second_file, "a title");

    VFS *const vfs = Vfs_Create(&g_HeapAllocator);
    CHECK(Result_Log(
        Vfs_MountDir(vfs, "", directory, 0, false), LOG_LEVEL_ERROR));

    VISITED_PATHS visited = { .count = 0 };
    Vfs_List(vfs, "", M_Visit, &visited);
    CHECK_EQ_INT(visited.count, 2);
    CHECK_EQ_STR(visited.paths[0], "Bgm.wav");
    CHECK_EQ_STR(visited.paths[1], "Title00.jpg");
    Mem_Free(visited.paths[0]);
    Mem_Free(visited.paths[1]);

    Vfs_Destroy(vfs);
    Mem_Free(second_file);
    Mem_Free(first_file);
    Mem_Free(directory);
}

TEST(mounting_a_missing_directory_fails)
{
    char *const directory = TestDir_Prepare("missing");
    char *const missing = TestDir_Join(directory, "does_not_exist");
    char *const file = TestDir_Join(directory, "file");
    M_Create(file, "x");

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    CHECK(!Result_Ignore(Vfs_MountDir(vfs, "", missing, 0, false)));
    CHECK(!Result_Ignore(Vfs_MountDir(vfs, "", missing, 0, true)));
    CHECK(!Result_Ignore(Vfs_MountDir(vfs, "", file, 0, true)));
    CHECK(!Vfs_Exists(vfs, "anything"));
    CHECK(!Result_Ignore(Vfs_Write(vfs, "anything", "x", 1)));

    Vfs_Destroy(vfs);
    CHECK_EQ_INT(test.live_blocks, 0);
    Mem_Free(file);
    Mem_Free(missing);
    Mem_Free(directory);
}

static void M_Join(void *const user, const char *const name)
{
    char **const joined = user;
    char *const next = Str_Format("%s%s;", *joined, name);
    Mem_Free(*joined);
    *joined = next;
}

static char *M_List(const VFS *const vfs, const char *const dir)
{
    char *joined = Str_Dup("");
    Vfs_List(vfs, dir, M_Join, &joined);
    return joined;
}

TEST(a_writable_directory_takes_writes_that_later_lookups_find)
{
    char *const directory = TestDir_Prepare("writable");
    char *const user = TestDir_Join(directory, "user");
    char *const replay = TestDir_Join(user, "Replay");
    SDL_CreateDirectory(user);
    SDL_CreateDirectory(replay);
    char *const old = TestDir_Join(replay, "TH6_01.rpy");
    M_Create(old, "old");

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    CHECK(Result_Ignore(Vfs_MountDir(vfs, "", user, 0, true)));
    CHECK(Result_Ignore(Vfs_Write(vfs, "replay/th6_01.rpy", "new", 3)));
    CHECK(Result_Ignore(Vfs_Write(vfs, "replay/th6_udAb.rpy", "ud", 2)));
    CHECK(Result_Ignore(Vfs_Write(vfs, "score.dat", "s", 1)));
    char *const listed = M_List(vfs, "REPLAY");
    CHECK_EQ_STR(listed, "TH6_01.rpy;th6_udAb.rpy;");
    READER reader;
    CHECK(Result_Ignore(
        Vfs_Open(vfs, "replay/th6_01.rpy", VFS_IN_MEMORY, &reader)));
    CHECK(M_ReadsAs(&reader, "new"));
    Reader_Close(&reader);
    char *const root = M_List(vfs, "");
    CHECK_EQ_STR(root, "score.dat;");

    Vfs_Destroy(vfs);
    CHECK_EQ_INT(test.live_blocks, 0);
    Mem_Free(root);
    Mem_Free(listed);
    Mem_Free(old);
    Mem_Free(replay);
    Mem_Free(user);
    Mem_Free(directory);
}

TEST(a_mount_point_limits_a_source_to_its_subtree)
{
    char *const directory = TestDir_Prepare("mount_at");
    char *const low = TestDir_Join(directory, "low");
    char *const high = TestDir_Join(directory, "high");
    SDL_CreateDirectory(low);
    SDL_CreateDirectory(high);
    char *const low_file = TestDir_Join(low, "a.txt");
    char *const high_file = TestDir_Join(high, "A.txt");
    char *const outside = TestDir_Join(high, "b.txt");
    M_Create(low_file, "low");
    M_Create(high_file, "high");
    M_Create(outside, "outside");

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    CHECK(Result_Ignore(Vfs_MountDir(vfs, "", low, 0, true)));
    CHECK(Result_Ignore(Vfs_MountDir(vfs, "replay", high, 1, false)));
    CHECK(Vfs_Exists(vfs, "replay/b.txt"));
    CHECK(!Vfs_Exists(vfs, "b.txt"));
    char *const listed = M_List(vfs, "replay");
    CHECK_EQ_STR(listed, "A.txt;b.txt;");
    CHECK(Result_Ignore(Vfs_Write(vfs, "replay/c.txt", "c", 1)));
    char *const written = TestDir_Join(low, "replay/c.txt");
    CHECK(SDL_GetPathInfo(written, nullptr));

    Vfs_Destroy(vfs);
    CHECK_EQ_INT(test.live_blocks, 0);
    Mem_Free(written);
    Mem_Free(listed);
    Mem_Free(outside);
    Mem_Free(high_file);
    Mem_Free(low_file);
    Mem_Free(high);
    Mem_Free(low);
    Mem_Free(directory);
}

TEST(a_listing_takes_each_name_from_the_highest_source_that_has_it)
{
    char *const directory = TestDir_Prepare("list_priority");
    char *const low = TestDir_Join(directory, "low");
    char *const low_replay = TestDir_Join(low, "replay");
    char *const high = TestDir_Join(directory, "high");
    SDL_CreateDirectory(low);
    SDL_CreateDirectory(low_replay);
    SDL_CreateDirectory(high);
    char *const files[] = {
        TestDir_Join(low_replay, "a.txt"),
        TestDir_Join(low_replay, "c.txt"),
        TestDir_Join(high, "A.txt"),
        TestDir_Join(high, "b.txt"),
    };
    for (size_t i = 0; i < ARRAY_SIZE(files); i++) {
        M_Create(files[i], "x");
    }

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    CHECK(Result_Ignore(Vfs_MountDir(vfs, "", low, 0, false)));
    CHECK(Result_Ignore(Vfs_MountDir(vfs, "replay", high, 1, false)));
    char *const listed = M_List(vfs, "replay");
    CHECK_EQ_STR(listed, "A.txt;b.txt;c.txt;");

    Vfs_Destroy(vfs);
    CHECK_EQ_INT(test.live_blocks, 0);
    Mem_Free(listed);
    for (size_t i = 0; i < ARRAY_SIZE(files); i++) {
        Mem_Free(files[i]);
    }
    Mem_Free(high);
    Mem_Free(low_replay);
    Mem_Free(low);
    Mem_Free(directory);
}

TEST(a_write_below_a_mount_point_lands_in_its_directory)
{
    char *const directory = TestDir_Prepare("write_at");
    char *const root = TestDir_Join(directory, "root");
    char *const user = TestDir_Join(directory, "user");
    SDL_CreateDirectory(root);
    SDL_CreateDirectory(user);

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    VFS *const vfs = Vfs_Create(&test.allocator);
    CHECK(Result_Ignore(Vfs_MountDir(vfs, "", root, 0, true)));
    CHECK(Result_Ignore(Vfs_MountDir(vfs, "save", user, 1, true)));
    CHECK(Result_Ignore(Vfs_Write(vfs, "save/slot/a.txt", "a", 1)));
    char *const written = TestDir_Join(user, "slot/a.txt");
    char *const wrong = TestDir_Join(root, "save");
    CHECK(SDL_GetPathInfo(written, nullptr));
    CHECK(!SDL_GetPathInfo(wrong, nullptr));
    CHECK(Vfs_Exists(vfs, "save/slot/a.txt"));

    Vfs_Destroy(vfs);
    CHECK_EQ_INT(test.live_blocks, 0);
    Mem_Free(wrong);
    Mem_Free(written);
    Mem_Free(user);
    Mem_Free(root);
    Mem_Free(directory);
}
