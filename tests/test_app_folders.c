#include "core/app/folders.h"

#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/file.h"

#include "test.h"
#include "test_dir.h"

#include <SDL3/SDL.h>

static void M_WriteFile(const char *const dir, const char *const name)
{
    char *const path = TestDir_Join(dir, name);
    CHECK(SDL_SaveFile(path, "", 0));
    Mem_Free(path);
}

static bool M_IsDir(const char *const path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

// macOS keeps the platform folders in ~/Library/Application Support and
// ignores XDG_DATA_HOME, so the tests of the platform folders would write
// there.
#ifndef __APPLE__
static char *M_PlatformDir(const char *const name)
{
    return Str_Format("%s/opentouhou/%s/", getenv("XDG_DATA_HOME"), name);
}
#endif

// Finds the folders for a program in dir, with portable.txt there when
// portable is set and with the --user-dir argument user_dir.
static void M_Find(
    const char *const dir, const bool portable, const char *const user_dir,
    APP_FOLDERS *const out)
{
    if (portable) {
        M_WriteFile(dir, "portable.txt");
    }
    char *const exe_dir = Str_Format("%s/", dir);
    const APP_FOLDERS_INPUT input = {
        .exe_dir = exe_dir,
        .program = "/somewhere/oth06",
        .user_dir = user_dir,
    };
    CHECK(Result_Log(AppFolders_Find(&input, "th06", out), LOG_LEVEL_ERROR));
    Mem_Free(exe_dir);
}

static void M_CheckPatchesDir(
    const APP_FOLDERS *const folders, const char *const expected)
{
    char *dir;
    CHECK(Result_Log(AppFolders_GetPatchesDir(folders, &dir), LOG_LEVEL_ERROR));
    CHECK_EQ_STR(dir, expected);
    CHECK(M_IsDir(dir));
    Mem_Free(dir);
}

#ifndef __APPLE__
TEST(default_mode_uses_the_platform_folders)
{
    char *const dir = TestDir_Prepare("default");
    APP_FOLDERS folders;
    M_Find(dir, false, nullptr, &folders);
    CHECK_EQ_INT(folders.mode, APP_FOLDERS_DEFAULT);
    char *const program_dir = Str_Format("%s/", dir);
    CHECK_EQ_STR(folders.program_dir, program_dir);
    CHECK_EQ_STR(folders.data_dir, program_dir);
    char *const user_dir = M_PlatformDir("th06");
    CHECK_EQ_STR(folders.user_dir, user_dir);
    char *const patches_dir = M_PlatformDir("patches");
    M_CheckPatchesDir(&folders, patches_dir);
    CHECK_EQ_STR(folders.program_name, "oth06");
    AppFolders_Free(&folders);
    Mem_Free(patches_dir);
    Mem_Free(user_dir);
    Mem_Free(program_dir);
    Mem_Free(dir);
}
#endif

TEST(portable_mode_keeps_everything_in_new_program_subfolders)
{
    char *const dir = TestDir_Prepare("portable");
    APP_FOLDERS folders;
    M_Find(dir, true, nullptr, &folders);
    CHECK_EQ_INT(folders.mode, APP_FOLDERS_PORTABLE);
    char *const user_dir = Str_Format("%s/userdata/", dir);
    CHECK_EQ_STR(folders.user_dir, user_dir);
    CHECK(M_IsDir(user_dir));
    char *const patches_dir = Str_Format("%s/patches/", dir);
    M_CheckPatchesDir(&folders, patches_dir);
    char *const data_dir = Str_Format("%s/", dir);
    CHECK_EQ_STR(folders.data_dir, data_dir);
    AppFolders_Free(&folders);
    Mem_Free(data_dir);
    Mem_Free(patches_dir);
    Mem_Free(user_dir);
    Mem_Free(dir);
}

#ifndef __APPLE__
TEST(user_dir_moves_only_the_user_folder)
{
    char *const dir = TestDir_Prepare("user_dir");
    char *const user_arg = Str_Format("%s/mine/th06", dir);
    APP_FOLDERS folders;
    M_Find(dir, false, user_arg, &folders);
    CHECK_EQ_INT(folders.mode, APP_FOLDERS_USER_DIR);
    char *const user_dir = Str_Format("%s/", user_arg);
    CHECK_EQ_STR(folders.user_dir, user_dir);
    CHECK(M_IsDir(user_dir));
    char *const patches_dir = M_PlatformDir("patches");
    M_CheckPatchesDir(&folders, patches_dir);
    AppFolders_Free(&folders);
    Mem_Free(patches_dir);
    Mem_Free(user_dir);
    Mem_Free(user_arg);
    Mem_Free(dir);
}
#endif

TEST(user_dir_wins_over_portable_mode_but_keeps_its_patches_folder)
{
    char *const dir = TestDir_Prepare("both");
    char *const user_arg = Str_Format("%s/mine/", dir);
    APP_FOLDERS folders;
    M_Find(dir, true, user_arg, &folders);
    CHECK_EQ_INT(folders.mode, APP_FOLDERS_USER_DIR_PORTABLE);
    CHECK_EQ_STR(folders.user_dir, user_arg);
    char *const userdata = Str_Format("%s/userdata", dir);
    CHECK(!File_Exists(userdata));
    char *const patches_dir = Str_Format("%s/patches/", dir);
    M_CheckPatchesDir(&folders, patches_dir);
    AppFolders_Free(&folders);
    Mem_Free(patches_dir);
    Mem_Free(userdata);
    Mem_Free(user_arg);
    Mem_Free(dir);
}

TEST(data_argument_wins_over_the_program_folder)
{
    char *const dir = TestDir_Prepare("data");
    char *const exe_dir = Str_Format("%s/", dir);
    const APP_FOLDERS_INPUT input = {
        .exe_dir = exe_dir,
        .data_dir = "/games/th06",
    };
    APP_FOLDERS folders;
    CHECK(
        Result_Log(AppFolders_Find(&input, "th06", &folders), LOG_LEVEL_ERROR));
    CHECK_EQ_STR(folders.data_dir, "/games/th06");
    CHECK_EQ_STR(folders.program_name, "th06");
    AppFolders_Free(&folders);
    Mem_Free(exe_dir);
    Mem_Free(dir);
}

TEST(a_user_folder_that_cannot_be_made_fails_and_keeps_the_others)
{
    char *const dir = TestDir_Prepare("blocked");
    M_WriteFile(dir, "file");
    char *const user_arg = Str_Format("%s/file/th06", dir);
    APP_FOLDERS folders;
    char *const exe_dir = Str_Format("%s/", dir);
    const APP_FOLDERS_INPUT input = {
        .exe_dir = exe_dir,
        .user_dir = user_arg,
    };
    const RESULT result = AppFolders_Find(&input, "th06", &folders);
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    CHECK(folders.user_dir == nullptr);
    CHECK_EQ_STR(folders.data_dir, exe_dir);
    AppFolders_Free(&folders);
    Mem_Free(exe_dir);
    Mem_Free(user_arg);
    Mem_Free(dir);
}

#ifdef __linux__
TEST(an_appimage_runs_from_the_folder_of_its_file)
{
    char *const dir = TestDir_Prepare("appimage");
    M_WriteFile(dir, "portable.txt");
    char *const appimage = Str_Format("%s/oth06-0.1.0.AppImage", dir);
    const APP_FOLDERS_INPUT input = {
        .exe_dir = "/tmp/.mount_oth06x/usr/bin/",
        .appimage = appimage,
        .appdir = "/tmp/.mount_oth06x",
        .program = "/tmp/.mount_oth06x/AppRun",
    };
    APP_FOLDERS folders;
    CHECK(
        Result_Log(AppFolders_Find(&input, "th06", &folders), LOG_LEVEL_ERROR));
    char *const program_dir = Str_Format("%s/", dir);
    CHECK_EQ_STR(folders.program_dir, program_dir);
    CHECK_EQ_STR(folders.data_dir, program_dir);
    CHECK_EQ_STR(folders.program_name, "oth06-0.1.0.AppImage");
    CHECK_EQ_INT(folders.mode, APP_FOLDERS_PORTABLE);
    AppFolders_Free(&folders);
    Mem_Free(program_dir);
    Mem_Free(appimage);
    Mem_Free(dir);
}

TEST(appimage_counts_only_inside_its_mount)
{
    const char *const appimages[][2] = {
        { "/games/oth06.AppImage", "/tmp/.mount_other" },
        { "oth06.AppImage", "/opt" },
        { "/games/oth06.AppImage", nullptr },
    };
    for (size_t i = 0; i < sizeof(appimages) / sizeof(appimages[0]); i++) {
        const APP_FOLDERS_INPUT input = {
            .exe_dir = "/opt/oth06/",
            .appimage = appimages[i][0],
            .appdir = appimages[i][1],
            .program = "./oth06",
        };
        APP_FOLDERS folders;
        Result_Ignore(AppFolders_Find(&input, "th06", &folders));
        CHECK_EQ_STR(folders.program_dir, "/opt/oth06/");
        CHECK_EQ_STR(folders.program_name, "oth06");
        AppFolders_Free(&folders);
    }
}
#endif

TEST(mode_names_differ)
{
    CHECK_EQ_STR(AppFolders_GetModeName(APP_FOLDERS_DEFAULT), "default");
    CHECK(Str_StartsWith(
        AppFolders_GetModeName(APP_FOLDERS_PORTABLE), "portable"));
    CHECK_EQ_STR(AppFolders_GetModeName(APP_FOLDERS_USER_DIR), "--user-dir");
    CHECK(Str_StartsWith(
        AppFolders_GetModeName(APP_FOLDERS_USER_DIR_PORTABLE), "--user-dir, "));
}
