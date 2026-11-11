#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/io/file.h"
#include "core/io/path.h"

#include "test.h"
#include "test_allocator.h"

#include <SDL3/SDL.h>

TEST(join_adds_one_separator)
{
    const char *const cases[][3] = {
        { "data", "th06.dat", "data/th06.dat" },
        { "data/", "th06.dat", "data/th06.dat" },
        { "C:\\th06\\", "th06.dat", "C:\\th06\\th06.dat" },
        { "", "th06.dat", "th06.dat" },
        { "user", "replay/th6_01.rpy", "user/replay/th6_01.rpy" },
        { "東方", "紅魔郷.dat", "東方/紅魔郷.dat" },
    };
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        char *const joined =
            Path_Join(&test.allocator, cases[i][0], cases[i][1]);
        CHECK_EQ_STR(joined, cases[i][2]);
        Allocator_Free(&test.allocator, joined, strlen(joined) + 1);
    }
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.size_errors, 0);
}

TEST(the_executable_directory_holds_the_executable)
{
    const char *const directory = Path_GetExeDir();
    CHECK(directory != nullptr && Str_EndsWith(directory, "/"));
    if (directory != nullptr) {
        char *const executable =
            Path_Join(&g_HeapAllocator, directory, "test_path");
        CHECK(File_Exists(executable));
        Allocator_Free(&g_HeapAllocator, executable, strlen(executable) + 1);
    }
}

// macOS keeps the user directory in ~/Library/Application Support and
// ignores XDG_DATA_HOME, so this test would write there.
#ifndef __APPLE__
TEST(the_user_directory_is_made_in_the_data_home)
{
    // meson points XDG_DATA_HOME into the build directory, so that the test
    // never writes to the real user directory.
    const char *const data_home = getenv("XDG_DATA_HOME");
    if (data_home == nullptr || strstr(data_home, "/tests/tmp/") == nullptr) {
        fprintf(stderr, "XDG_DATA_HOME must name a directory in tests/tmp/\n");
        abort();
    }
    char *directory;
    CHECK(Result_Log(Path_GetUserDir("th06", &directory), LOG_LEVEL_ERROR));
    char *const expected = Str_Format("%s/opentouhou/th06/", data_home);
    CHECK_EQ_STR(directory, expected);
    SDL_PathInfo info;
    CHECK(
        SDL_GetPathInfo(directory, &info)
        && info.type == SDL_PATHTYPE_DIRECTORY);
    Mem_Free(expected);
    Mem_Free(directory);
}

TEST(the_patches_folder_is_beside_the_user_directories)
{
    const char *const data_home = getenv("XDG_DATA_HOME");
    char *directory;
    CHECK(Result_Log(Path_GetPatchesDir(&directory), LOG_LEVEL_ERROR));
    char *const expected = Str_Format("%s/opentouhou/patches/", data_home);
    CHECK_EQ_STR(directory, expected);
    SDL_PathInfo info;
    CHECK(
        SDL_GetPathInfo(directory, &info)
        && info.type == SDL_PATHTYPE_DIRECTORY);
    Mem_Free(expected);
    Mem_Free(directory);
}
#endif
