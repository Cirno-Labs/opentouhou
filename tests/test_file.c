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

TEST(list_gives_every_name_sorted)
{
    char *const directory = TestDir_Prepare("list");
    const char *const files[] = { "b.dat", "東方.txt", "A.TXT", "a.txt" };
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
        "A.TXT", "a.txt", "b.dat", "sub", "東方.txt",
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
