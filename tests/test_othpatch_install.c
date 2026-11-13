// SPDX-License-Identifier: GPL-3.0-only

#include "othpatch/install.h"

#include "patch_builder.h"
#include "test.h"
#include "test_dir.h"
#include "zip_builder.h"

#include <SDL3/SDL.h>
#include <sys/stat.h>

#define M_OLD_OTHPATCH "old othpatch"
// Holds the othpatch of the packages, which prints its version as othpatch
// does.
#define M_NEW_OTHPATCH "#!/bin/sh\necho \"othpatch 0.2.0\"\n"

typedef struct {
    char *dir;
    ARENA arena;
    ZIP_BUILDER_ARCHIVE package;
    // Holds the version of the othpatch in dir, or tells that it is a
    // development build.
    RELEASE_VERSION othpatch_version;
    bool othpatch_development;
    bool othpatch_replaced;
} M_CASE;

// Prepares test in place, because its arena must not move.
static void M_Prepare(M_CASE *const test, const char *const name)
{
    *test = (M_CASE) {
        .dir = TestDir_Prepare(name),
        .othpatch_version = { 0, 1, 0 },
    };
    Arena_Init(&test->arena, "install test", 1 << 20);
}

static RESULT M_Install(
    M_CASE *const test, const INSTALL_PLATFORM *const platform,
    const ZIP_BUILDER_ENTRY *const entries, const int count)
{
    test->package = ZipBuilder_Build(entries, count);
    return Install_Package(
        platform, test->dir, "th06", test->package.data, test->package.size,
        test->othpatch_development ? nullptr : &test->othpatch_version,
        &test->arena, &test->othpatch_replaced);
}

static void M_Close(M_CASE *const test)
{
    Mem_Free(test->package.data);
    Arena_Free(&test->arena);
    Mem_Free(test->dir);
}

static void M_CheckFile(
    const M_CASE *const test, const char *const name,
    const char *const expected)
{
    char *const path = TestDir_Join(test->dir, name);
    char *const data = SDL_LoadFile(path, nullptr);
    if (data == nullptr) {
        TEST_FAIL("%s is missing", name);
    } else {
        CHECK_EQ_STR(data, expected);
    }
    SDL_free(data);
    Mem_Free(path);
}

static bool M_Exists(const M_CASE *const test, const char *const name)
{
    char *const path = TestDir_Join(test->dir, name);
    const bool exists = File_Exists(path);
    Mem_Free(path);
    return exists;
}

static uint32_t M_Mode(const M_CASE *const test, const char *const name)
{
    char *const path = TestDir_Join(test->dir, name);
    struct stat info = {};
    CHECK(stat(path, &info) == 0);
    Mem_Free(path);
    return info.st_mode & 0777;
}

static ZIP_BUILDER_ENTRY M_Entry(
    const char *const name, const char *const data, const uint32_t mode)
{
    return (ZIP_BUILDER_ENTRY) {
        .name = name,
        .data = data,
        .size = strlen(data),
        .mode = mode,
        .deflate = true,
    };
}

TEST(linux_package_replaces_the_appimage_and_othpatch)
{
    M_CASE test;
    M_Prepare(&test, "linux");
    PatchBuilder_Write(test.dir, "oth06.AppImage", "old game");
    PatchBuilder_Write(test.dir, "othpatch", M_OLD_OTHPATCH);
    PatchBuilder_Write(test.dir, "README.txt", "the user's notes");
    const ZIP_BUILDER_ENTRY entries[] = {
        M_Entry("oth06.AppImage", "new game", 0100644),
        M_Entry("othpatch", M_NEW_OTHPATCH, 0100644),
        M_Entry("README.txt", "readme", 0100644),
    };
    CHECK(Result_Log(
        M_Install(&test, &g_InstallLinux, entries, 3), LOG_LEVEL_ERROR));
    M_CheckFile(&test, "oth06.AppImage", "new game");
    M_CheckFile(&test, "othpatch", M_NEW_OTHPATCH);
    M_CheckFile(&test, "README.txt", "the user's notes");
    CHECK(test.othpatch_replaced);
    CHECK_EQ_INT(test.othpatch_version.minor, 2);
    CHECK_EQ_INT(M_Mode(&test, "oth06.AppImage"), 0755);
    CHECK_EQ_INT(M_Mode(&test, "othpatch"), 0755);
    CHECK(!M_Exists(&test, "oth06.AppImage.new"));
    CHECK(!M_Exists(&test, "oth06.AppImage.old"));
    M_Close(&test);
}

TEST(same_or_older_othpatch_is_left_alone)
{
    const RELEASE_VERSION versions[] = { { 0, 2, 0 }, { 1, 0, 0 } };
    for (size_t i = 0; i < ARRAY_SIZE(versions); i++) {
        M_CASE test;
        M_Prepare(&test, "same_othpatch");
        test.othpatch_version = versions[i];
        PatchBuilder_Write(test.dir, "othpatch", M_OLD_OTHPATCH);
        char *const path = TestDir_Join(test.dir, "othpatch");
        CHECK(chmod(path, 0700) == 0);
        const ZIP_BUILDER_ENTRY entries[] = {
            M_Entry("oth06.AppImage", "new game", 0),
            M_Entry("othpatch", M_NEW_OTHPATCH, 0),
        };
        CHECK(Result_Log(
            M_Install(&test, &g_InstallLinux, entries, 2), LOG_LEVEL_ERROR));
        CHECK(!test.othpatch_replaced);
        CHECK(Release_CompareVersions(test.othpatch_version, versions[i]) == 0);
        M_CheckFile(&test, "othpatch", M_OLD_OTHPATCH);
        CHECK_EQ_INT(M_Mode(&test, "othpatch"), 0700);
        CHECK(!M_Exists(&test, "othpatch.new"));
        M_CheckFile(&test, "oth06.AppImage", "new game");
        Mem_Free(path);
        M_Close(&test);
    }
}

TEST(development_othpatch_is_left_alone)
{
    M_CASE test;
    M_Prepare(&test, "development_othpatch");
    test.othpatch_development = true;
    PatchBuilder_Write(test.dir, "othpatch", M_OLD_OTHPATCH);
    const ZIP_BUILDER_ENTRY entries[] = {
        M_Entry("oth06.AppImage", "new game", 0),
        M_Entry("othpatch", M_NEW_OTHPATCH, 0),
    };
    CHECK(Result_Log(
        M_Install(&test, &g_InstallLinux, entries, 2), LOG_LEVEL_ERROR));
    CHECK(!test.othpatch_replaced);
    M_CheckFile(&test, "othpatch", M_OLD_OTHPATCH);
    M_CheckFile(&test, "oth06.AppImage", "new game");
    M_Close(&test);
}

TEST(othpatch_without_a_version_is_left_alone)
{
    M_CASE test;
    M_Prepare(&test, "othpatch_without_a_version");
    PatchBuilder_Write(test.dir, "othpatch", M_OLD_OTHPATCH);
    const ZIP_BUILDER_ENTRY entries[] = {
        M_Entry("oth06.AppImage", "new game", 0),
        M_Entry("othpatch", "#!/bin/sh\nexit 1\n", 0),
    };
    CHECK(Result_Log(
        M_Install(&test, &g_InstallLinux, entries, 2), LOG_LEVEL_ERROR));
    CHECK(!test.othpatch_replaced);
    M_CheckFile(&test, "othpatch", M_OLD_OTHPATCH);
    CHECK(!M_Exists(&test, "othpatch.new"));
    M_Close(&test);
}

TEST(othpatch_installs_from_its_own_release)
{
    const INSTALL_PLATFORM *const platforms[] = { &g_InstallLinux,
                                                  &g_InstallWindows };
    for (size_t i = 0; i < ARRAY_SIZE(platforms); i++) {
        M_CASE test;
        M_Prepare(&test, "othpatch_release");
        const char *const name = platforms[i]->othpatch;
        PatchBuilder_Write(test.dir, name, M_OLD_OTHPATCH);
        CHECK(Result_Log(
            Install_Othpatch(
                platforms[i], test.dir, (const uint8_t *)"new", 3, &test.arena),
            LOG_LEVEL_ERROR));
        M_CheckFile(&test, name, "new");
        CHECK_EQ_INT(M_Mode(&test, name), 0755);
        char *const staged = Str_Format("%s.new", name);
        char *const old = Str_Format("%s.old", name);
        CHECK(!M_Exists(&test, staged));
        CHECK(!M_Exists(&test, old));
        Mem_Free(old);
        Mem_Free(staged);
        M_Close(&test);
    }
}

TEST(windows_package_moves_the_old_files_aside)
{
    M_CASE test;
    M_Prepare(&test, "windows");
    PatchBuilder_Write(test.dir, "oth06.exe", "old game");
    PatchBuilder_Write(test.dir, "othpatch.exe", M_OLD_OTHPATCH);
    PatchBuilder_Write(test.dir, "oth06.exe.old", "from an earlier update");
    const ZIP_BUILDER_ENTRY entries[] = {
        M_Entry("oth06.exe", "new game", 0),
        M_Entry("othpatch.exe", M_NEW_OTHPATCH, 0),
    };
    CHECK(Result_Log(
        M_Install(&test, &g_InstallWindows, entries, 2), LOG_LEVEL_ERROR));
    M_CheckFile(&test, "oth06.exe", "new game");
    M_CheckFile(&test, "othpatch.exe", M_NEW_OTHPATCH);
    CHECK(test.othpatch_replaced);
    CHECK(!M_Exists(&test, "oth06.exe.old"));
    CHECK(!M_Exists(&test, "oth06.exe.new"));
    CHECK(!M_Exists(&test, "othpatch.exe.old"));
    M_Close(&test);
}

TEST(macos_package_replaces_the_whole_bundle)
{
    M_CASE test;
    M_Prepare(&test, "macos");
    PatchBuilder_Write(test.dir, "oth06.app/Contents/MacOS/oth06", "old game");
    PatchBuilder_Write(test.dir, "oth06.app/Contents/Resources/gone", "x");
    PatchBuilder_Write(test.dir, "othpatch", M_OLD_OTHPATCH);
    const ZIP_BUILDER_ENTRY entries[] = {
        M_Entry("oth06.app/", "", 040755),
        M_Entry("oth06.app/Contents/", "", 040755),
        M_Entry("oth06.app/Contents/MacOS/oth06", "new game", 0100755),
        M_Entry("oth06.app/Contents/Info.plist", "plist", 0100644),
        M_Entry("othpatch", M_NEW_OTHPATCH, 0100755),
        M_Entry("__MACOSX/oth06.app/._Info.plist", "fork", 0100644),
    };
    CHECK(Result_Log(
        M_Install(&test, &g_InstallMacOs, entries, 6), LOG_LEVEL_ERROR));
    M_CheckFile(&test, "oth06.app/Contents/MacOS/oth06", "new game");
    M_CheckFile(&test, "oth06.app/Contents/Info.plist", "plist");
    CHECK_EQ_INT(M_Mode(&test, "oth06.app/Contents/MacOS/oth06"), 0755);
    CHECK_EQ_INT(M_Mode(&test, "oth06.app/Contents/Info.plist"), 0644);
    CHECK(!M_Exists(&test, "oth06.app/Contents/Resources/gone"));
    CHECK(!M_Exists(&test, "oth06.app.old"));
    CHECK(!M_Exists(&test, "oth06.app.new"));
    CHECK(!M_Exists(&test, "__MACOSX"));
    M_CheckFile(&test, "othpatch", M_NEW_OTHPATCH);
    M_Close(&test);
}

TEST(bundle_path_out_of_the_bundle_fails)
{
    M_CASE test;
    M_Prepare(&test, "unsafe");
    PatchBuilder_Write(test.dir, "oth06.app/Contents/MacOS/oth06", "old game");
    const ZIP_BUILDER_ENTRY entries[] = {
        M_Entry("oth06.app/../evil", "x", 0100644),
        M_Entry("othpatch", M_NEW_OTHPATCH, 0100755),
    };
    CHECK(!Result_Ignore(M_Install(&test, &g_InstallMacOs, entries, 2)));
    M_CheckFile(&test, "oth06.app/Contents/MacOS/oth06", "old game");
    CHECK(!M_Exists(&test, "evil"));
    M_Close(&test);
}

TEST(bundle_with_a_symbolic_link_fails)
{
    M_CASE test;
    M_Prepare(&test, "link");
    const ZIP_BUILDER_ENTRY entries[] = {
        M_Entry("oth06.app/Contents/link", "/etc", 0120755),
        M_Entry("othpatch", M_NEW_OTHPATCH, 0100755),
    };
    const RESULT result = M_Install(&test, &g_InstallMacOs, entries, 2);
    CHECK(!IS_OK(result) && strstr(result.message, "symbolic") != nullptr);
    Result_Ignore(result);
    M_Close(&test);
}

TEST(package_without_the_game_changes_nothing)
{
    M_CASE test;
    M_Prepare(&test, "no_game");
    PatchBuilder_Write(test.dir, "oth06.AppImage", "old game");
    PatchBuilder_Write(test.dir, "othpatch", M_OLD_OTHPATCH);
    const ZIP_BUILDER_ENTRY entries[] = {
        M_Entry("oth07.AppImage", "other game", 0),
        M_Entry("othpatch", M_NEW_OTHPATCH, 0),
    };
    const RESULT result = M_Install(&test, &g_InstallLinux, entries, 2);
    CHECK(
        !IS_OK(result) && strstr(result.message, "oth06.AppImage") != nullptr);
    Result_Ignore(result);
    M_CheckFile(&test, "oth06.AppImage", "old game");
    M_CheckFile(&test, "othpatch", M_OLD_OTHPATCH);
    M_Close(&test);
}

TEST(package_without_othpatch_installs_the_game_only)
{
    M_CASE test;
    M_Prepare(&test, "no_othpatch");
    PatchBuilder_Write(test.dir, "oth06.AppImage", "old game");
    PatchBuilder_Write(test.dir, "othpatch", M_OLD_OTHPATCH);
    const ZIP_BUILDER_ENTRY entries[] = {
        M_Entry("oth06.AppImage", "new game", 0),
    };
    CHECK(Result_Log(
        M_Install(&test, &g_InstallLinux, entries, 1), LOG_LEVEL_ERROR));
    M_CheckFile(&test, "oth06.AppImage", "new game");
    M_CheckFile(&test, "othpatch", M_OLD_OTHPATCH);
    M_Close(&test);
}

TEST(leftovers_are_removed_and_other_files_stay)
{
    M_CASE test;
    M_Prepare(&test, "leftovers");
    PatchBuilder_Write(test.dir, "othpatch.exe.old", "x");
    PatchBuilder_Write(test.dir, "oth06.exe.old", "x");
    PatchBuilder_Write(test.dir, "oth06.exe.new", "x");
    PatchBuilder_Write(test.dir, "notes.old", "the user's");
    PatchBuilder_Write(test.dir, "oth06.app.old/Contents/MacOS/oth06", "x");
    const char *const games[] = { "th06" };
    Install_RemoveLeftovers(&g_InstallWindows, test.dir, games, 1, &test.arena);
    CHECK(!M_Exists(&test, "othpatch.exe.old"));
    CHECK(!M_Exists(&test, "oth06.exe.old"));
    CHECK(!M_Exists(&test, "oth06.exe.new"));
    CHECK(M_Exists(&test, "notes.old"));
    Install_RemoveLeftovers(&g_InstallMacOs, test.dir, games, 1, &test.arena);
    CHECK(!M_Exists(&test, "oth06.app.old"));
    M_Close(&test);
}

TEST(game_path_names_the_executable)
{
    ARENA arena;
    Arena_Init(&arena, "install test", 1 << 16);
    CHECK_EQ_STR(
        Install_GetGamePath(&g_InstallLinux, "/a/", "th06", &arena),
        "/a/oth06.AppImage");
    CHECK_EQ_STR(
        Install_GetGamePath(&g_InstallWindows, "/a", "th06", &arena),
        "/a/oth06.exe");
    CHECK_EQ_STR(
        Install_GetGamePath(&g_InstallMacOs, "/a", "th06", &arena),
        "/a/oth06.app/Contents/MacOS/oth06");
    Arena_Free(&arena);
}
