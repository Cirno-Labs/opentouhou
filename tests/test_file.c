#include "core/base/utils.h"
#include "core/io/file.h"

#include "test.h"
#include "test_allocator.h"
#include "test_dir.h"

static void M_Create(const char *const path)
{
    if (!SDL_SaveFile(path, "x", 1)) {
        TEST_FAIL("cannot create %s: %s", path, SDL_GetError());
    }
}

TEST(exists_finds_files_and_directories)
{
    char *const directory = TestDir_Prepare("exists");
    char *const file = TestDir_Join(directory, "東方.dat");
    char *const missing = TestDir_Join(directory, "missing");
    M_Create(file);
    CHECK(File_Exists(file));
    CHECK(File_Exists(directory));
    CHECK(!File_Exists(missing));
    Mem_Free(missing);
    Mem_Free(file);
    Mem_Free(directory);
}

TEST(make_dir_creates_missing_parents)
{
    char *const directory = TestDir_Prepare("make_dir");
    char *const outer = TestDir_Join(directory, "outer");
    char *const inner = TestDir_Join(outer, "inner");
    CHECK(Result_Log(File_MakeDir(inner), LOG_LEVEL_ERROR));
    CHECK(File_Exists(inner));
    CHECK(Result_Log(File_MakeDir(inner), LOG_LEVEL_ERROR));
    Mem_Free(inner);
    Mem_Free(outer);
    Mem_Free(directory);
}

TEST(make_dir_fails_below_a_file)
{
    char *const directory = TestDir_Prepare("make_dir_file");
    char *const file = TestDir_Join(directory, "file");
    char *const below = TestDir_Join(file, "below");
    M_Create(file);
    const RESULT result = File_MakeDir(below);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    Mem_Free(below);
    Mem_Free(file);
    Mem_Free(directory);
}

TEST(get_type_tells_files_from_directories)
{
    char *const directory = TestDir_Prepare("get_type");
    char *const file = TestDir_Join(directory, "東方.dat");
    char *const missing = TestDir_Join(directory, "missing");
    M_Create(file);
    FILE_TYPE type = FILE_TYPE_OTHER;
    CHECK(Result_Log(File_GetType(file, &type), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(type, FILE_TYPE_FILE);
    CHECK(Result_Log(File_GetType(directory, &type), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(type, FILE_TYPE_DIR);
    char *const prefix = Str_Format("Cannot query %s: ", missing);
    const RESULT result = File_GetType(missing, &type);
    CHECK(!IS_OK(result) && Str_StartsWith(result.message, prefix));
    Result_Ignore(result);
    Mem_Free(prefix);
    Mem_Free(missing);
    Mem_Free(file);
    Mem_Free(directory);
}

static void M_CheckContent(const char *const path, const char *const expected)
{
    size_t size = 0;
    char *const data = SDL_LoadFile(path, &size);
    CHECK(data != nullptr && size == strlen(expected));
    if (data != nullptr) {
        CHECK(memcmp(data, expected, size) == 0);
    }
    SDL_free(data);
}

TEST(rename_moves_a_file)
{
    char *const directory = TestDir_Prepare("rename");
    char *const from = TestDir_Join(directory, "from");
    char *const to = TestDir_Join(directory, "to");
    M_Create(from);
    CHECK(Result_Log(File_Rename(from, to), LOG_LEVEL_ERROR));
    CHECK(!File_Exists(from));
    M_CheckContent(to, "x");
    Mem_Free(to);
    Mem_Free(from);
    Mem_Free(directory);
}

TEST(rename_replaces_the_target)
{
    char *const directory = TestDir_Prepare("rename_replace");
    char *const from = TestDir_Join(directory, "from");
    char *const to = TestDir_Join(directory, "to");
    CHECK(SDL_SaveFile(from, "new", 3));
    CHECK(SDL_SaveFile(to, "old", 3));
    CHECK(Result_Log(File_Rename(from, to), LOG_LEVEL_ERROR));
    CHECK(!File_Exists(from));
    M_CheckContent(to, "new");
    Mem_Free(to);
    Mem_Free(from);
    Mem_Free(directory);
}

TEST(rename_of_a_missing_file_fails)
{
    char *const directory = TestDir_Prepare("rename_missing");
    char *const from = TestDir_Join(directory, "from");
    char *const to = TestDir_Join(directory, "to");
    const RESULT result = File_Rename(from, to);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    CHECK(!File_Exists(to));
    Mem_Free(to);
    Mem_Free(from);
    Mem_Free(directory);
}

TEST(remove_deletes_a_file_and_an_empty_directory)
{
    char *const directory = TestDir_Prepare("remove");
    char *const file = TestDir_Join(directory, "file");
    char *const inner = TestDir_Join(directory, "inner");
    M_Create(file);
    CHECK(Result_Log(File_MakeDir(inner), LOG_LEVEL_ERROR));
    CHECK(Result_Log(File_Remove(file), LOG_LEVEL_ERROR));
    CHECK(Result_Log(File_Remove(inner), LOG_LEVEL_ERROR));
    CHECK(!File_Exists(file));
    CHECK(!File_Exists(inner));
    Mem_Free(inner);
    Mem_Free(file);
    Mem_Free(directory);
}

TEST(remove_of_a_missing_path_succeeds)
{
    char *const directory = TestDir_Prepare("remove_missing");
    char *const missing = TestDir_Join(directory, "missing");
    CHECK(Result_Log(File_Remove(missing), LOG_LEVEL_ERROR));
    Mem_Free(missing);
    Mem_Free(directory);
}

TEST(remove_of_a_directory_with_entries_fails)
{
    char *const directory = TestDir_Prepare("remove_full");
    char *const inner = TestDir_Join(directory, "inner");
    char *const file = TestDir_Join(inner, "file");
    CHECK(Result_Log(File_MakeDir(inner), LOG_LEVEL_ERROR));
    M_Create(file);
    const RESULT result = File_Remove(inner);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    CHECK(File_Exists(file));
    Mem_Free(file);
    Mem_Free(inner);
    Mem_Free(directory);
}

TEST(list_gives_every_name_sorted)
{
    char *const directory = TestDir_Prepare("list");
    // The names differ in more than case, because macOS and Windows file
    // systems ignore case by default; "C.TXT" before "a.txt" still shows
    // byte order.
    const char *const files[] = { "b.dat", "東方.txt", "C.TXT", "a.txt" };
    for (size_t i = 0; i < ARRAY_SIZE(files); i++) {
        char *const path = TestDir_Join(directory, files[i]);
        M_Create(path);
        Mem_Free(path);
    }
    char *const sub = TestDir_Join(directory, "sub");
    char *const inner = TestDir_Join(sub, "inner.dat");
    SDL_CreateDirectory(sub);
    M_Create(inner);

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    FILE_LIST list;
    CHECK(Result_Log(
        File_ListDir(directory, &test.allocator, &list), LOG_LEVEL_ERROR));
    const char *const expected[] = {
        "C.TXT", "a.txt", "b.dat", "sub", "東方.txt",
    };
    CHECK_EQ_INT(list.count, ARRAY_SIZE(expected));
    for (int32_t i = 0; i < list.count && i < (int32_t)ARRAY_SIZE(expected);
         i++) {
        CHECK_EQ_STR(list.items[i], expected[i]);
    }
    File_FreeList(&list);
    CHECK_EQ_INT(list.count, 0);
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.size_errors, 0);
    Mem_Free(inner);
    Mem_Free(sub);
    Mem_Free(directory);
}

TEST(list_of_an_empty_directory_is_empty)
{
    char *const directory = TestDir_Prepare("list_empty");
    FILE_LIST list;
    CHECK(Result_Log(
        File_ListDir(directory, &g_HeapAllocator, &list), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(list.count, 0);
    File_FreeList(&list);
    Mem_Free(directory);
}

TEST(list_of_a_missing_directory_fails)
{
    char *const directory = TestDir_Prepare("list_missing");
    char *const path = TestDir_Join(directory, "missing");
    char *const prefix = Str_Format("Cannot list %s: ", path);
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    FILE_LIST list;
    const RESULT result = File_ListDir(path, &test.allocator, &list);
    CHECK(!IS_OK(result) && Str_StartsWith(result.message, prefix));
    Result_Ignore(result);
    CHECK_EQ_INT(list.count, 0);
    File_FreeList(&list);
    CHECK_EQ_INT(test.live_blocks, 0);
    Mem_Free(prefix);
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(every_call_reaches_a_path_longer_than_max_path)
{
    char *directory = TestDir_Prepare("long_path");
    for (int i = 0; i < 10; i++) {
        char *const deeper =
            TestDir_Join(directory, "a_directory_with_a_long_name");
        Mem_Free(directory);
        directory = deeper;
    }
    CHECK(strlen(directory) > 260);
    CHECK(Result_Log(File_MakeDir(directory), LOG_LEVEL_ERROR));
    FILE_TYPE type = FILE_TYPE_OTHER;
    CHECK(Result_Log(File_GetType(directory, &type), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(type, FILE_TYPE_DIR);

    char *const written = TestDir_Join(directory, "東方紅魔郷.dat");
    char *const renamed = TestDir_Join(directory, "renamed.dat");
    TestDir_Write(written, "abc", 3);
    CHECK(Result_Log(File_Rename(written, renamed), LOG_LEVEL_ERROR));
    CHECK(!File_Exists(written));
    char *const data = TestDir_Read(renamed);
    CHECK(data != nullptr);
    if (data != nullptr) {
        CHECK_EQ_STR(data, "abc");
    }
    Mem_Free(data);

    FILE_LIST list;
    CHECK(Result_Log(
        File_ListDir(directory, &g_HeapAllocator, &list), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(list.count, 1);
    if (list.count == 1) {
        CHECK_EQ_STR(list.items[0], "renamed.dat");
    }
    File_FreeList(&list);

    CHECK(Result_Log(File_Remove(renamed), LOG_LEVEL_ERROR));
    CHECK(!File_Exists(renamed));
    CHECK(Result_Log(File_Remove(directory), LOG_LEVEL_ERROR));
    CHECK(!File_Exists(directory));
    Mem_Free(renamed);
    Mem_Free(written);
    Mem_Free(directory);
}
