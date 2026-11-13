#include "othpatch/game.h"

#include "fake_program.h"
#include "test.h"
#include "test_dir.h"

#ifdef _WIN32
    #define M_GAME "oth06.exe"
#else
    #define M_GAME "oth06"
#endif

static RESULT M_GetVersion(
    const char *const name, const char *const output, const int status,
    char version[GAME_VERSION_SIZE])
{
    char *const dir = TestDir_Prepare(name);
    FakeProgram_Write(dir, M_GAME, output, status);
    char *const path = TestDir_Join(dir, M_GAME);
    const RESULT result = Game_GetVersion(path, version);
    Mem_Free(path);
    Mem_Free(dir);
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
        M_GetVersion("plain", "th06 0.1.0\n", 0, version), LOG_LEVEL_ERROR));
    CHECK_EQ_STR(version, "0.1.0");
}

TEST(version_ignores_a_carriage_return_and_later_lines)
{
    char version[GAME_VERSION_SIZE];
    CHECK(Result_Log(
        M_GetVersion("windows", "th06 1.2.3-4-gabcdef0\r\nmore\n", 0, version),
        LOG_LEVEL_ERROR));
    CHECK_EQ_STR(version, "1.2.3-4-gabcdef0");
}

TEST(game_that_fails_has_no_version)
{
    char version[GAME_VERSION_SIZE];
    const RESULT result = M_GetVersion("status", "th06 0.1.0\n", 3, version);
    CHECK(!IS_OK(result) && strstr(result.message, "status 3") != nullptr);
    Result_Ignore(result);
    CHECK_EQ_STR(version, "");
}

TEST(game_that_prints_no_version_fails)
{
    char version[GAME_VERSION_SIZE];
    const RESULT result = M_GetVersion("silent", "", 0, version);
    CHECK(!IS_OK(result) && strstr(result.message, "no version") != nullptr);
    Result_Ignore(result);
}

TEST(missing_game_fails)
{
    char *const dir = TestDir_Prepare("missing");
    char *const path = TestDir_Join(dir, M_GAME);
    char version[GAME_VERSION_SIZE];
    CHECK(!Result_Ignore(Game_GetVersion(path, version)));
    Mem_Free(path);
    Mem_Free(dir);
}
