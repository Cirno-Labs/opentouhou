// SPDX-License-Identifier: GPL-3.0-only

#include "othpatch/game.h"

#include "patch_builder.h"
#include "test.h"
#include "test_dir.h"

#include <sys/stat.h>

// Writes an executable shell script that runs body, and returns its path.
// Mem_Free releases the path.
static char *M_WriteGame(const char *const name, const char *const body)
{
    char *const dir = TestDir_Prepare(name);
    char *const text = Str_Format("#!/bin/sh\n%s\n", body);
    PatchBuilder_Write(dir, "oth06", text);
    char *const path = TestDir_Join(dir, "oth06");
    CHECK(chmod(path, 0755) == 0);
    Mem_Free(text);
    Mem_Free(dir);
    return path;
}

static RESULT M_GetVersion(
    const char *const name, const char *const body,
    char version[GAME_VERSION_SIZE])
{
    char *const path = M_WriteGame(name, body);
    const RESULT result = Game_GetVersion(path, version);
    Mem_Free(path);
    return result;
}

TEST(product_name_puts_an_o_before_the_game_id)
{
    char name[GAME_NAME_SIZE];
    Game_GetProductName("th06", name);
    CHECK_EQ_STR(name, "oth06");
}

TEST(version_is_the_last_word_of_the_line)
{
    char version[GAME_VERSION_SIZE];
    CHECK(Result_Log(
        M_GetVersion("plain", "echo \"th06 0.1.0\"", version),
        LOG_LEVEL_ERROR));
    CHECK_EQ_STR(version, "0.1.0");
}

TEST(version_ignores_a_carriage_return_and_later_lines)
{
    char version[GAME_VERSION_SIZE];
    CHECK(Result_Log(
        M_GetVersion(
            "windows", "printf 'th06 1.2.3-4-gabcdef0\\r\\nmore\\n'", version),
        LOG_LEVEL_ERROR));
    CHECK_EQ_STR(version, "1.2.3-4-gabcdef0");
}

TEST(game_that_fails_has_no_version)
{
    char version[GAME_VERSION_SIZE];
    const RESULT result =
        M_GetVersion("status", "echo \"th06 0.1.0\"; exit 3", version);
    CHECK(!IS_OK(result) && strstr(result.message, "status 3") != nullptr);
    Result_Ignore(result);
    CHECK_EQ_STR(version, "");
}

TEST(game_that_prints_no_version_fails)
{
    char version[GAME_VERSION_SIZE];
    const RESULT result = M_GetVersion("silent", "true", version);
    CHECK(!IS_OK(result) && strstr(result.message, "no version") != nullptr);
    Result_Ignore(result);
}

TEST(missing_game_fails)
{
    char *const dir = TestDir_Prepare("missing");
    char *const path = TestDir_Join(dir, "oth06");
    char version[GAME_VERSION_SIZE];
    CHECK(!Result_Ignore(Game_GetVersion(path, version)));
    Mem_Free(path);
    Mem_Free(dir);
}
