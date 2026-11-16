#include "othpatch/game.h"

#include "core/base/arena.h"
#include "core/base/memory.h"
#include "core/io/file.h"
#include "core/io/path.h"

#include "fake_program.h"
#include "test.h"
#include "test_dir.h"

#include <stdio.h>
#include <string.h>

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

// Returns what was written to out, and closes it. Mem_Free releases the
// text.
static char *M_Read(FILE *const out)
{
    const long size = ftell(out);
    rewind(out);
    char *const text = Mem_AllocZ((size_t)size + 1);
    CHECK(fread(text, 1, (size_t)size, out) == (size_t)size);
    fclose(out);
    return text;
}

// Writes an empty file at dir/name, with the directories above it. The
// games find functions never run it, so its contents do not matter.
static void M_WriteFile(const char *const dir, const char *const name)
{
    char *const path = TestDir_Join(dir, name);
    TestDir_Write(path, "", 0);
    Mem_Free(path);
}

TEST(beside_finds_every_known_form)
{
    char *const dir = TestDir_Prepare("beside_forms");
    M_WriteFile(dir, "oth06");
    M_WriteFile(dir, "oth07.exe");
    M_WriteFile(dir, "oth08.AppImage");
    M_WriteFile(dir, "oth09.app");
    ARENA arena;
    Arena_Init(&arena, "test", 1 << 16);
    const char *games[APP_GAME_COUNT];
    int32_t count = 0;
    FILE *const out = tmpfile();
    Game_FindBeside(games, &count, &arena, dir, out);
    CHECK_EQ_INT(count, 4);
    CHECK_EQ_STR(games[0], "th06");
    CHECK_EQ_STR(games[1], "th07");
    CHECK_EQ_STR(games[2], "th08");
    CHECK_EQ_STR(games[3], "th09");
    fclose(out);
    Arena_Free(&arena);
    Mem_Free(dir);
}

TEST(beside_ignores_a_non_matching_name)
{
    char *const dir = TestDir_Prepare("beside_non_matching");
    M_WriteFile(dir, "the_game");
    ARENA arena;
    Arena_Init(&arena, "test", 1 << 16);
    const char *games[APP_GAME_COUNT];
    int32_t count = 0;
    FILE *const out = tmpfile();
    Game_FindBeside(games, &count, &arena, dir, out);
    CHECK_EQ_INT(count, 0);
    fclose(out);
    Arena_Free(&arena);
    Mem_Free(dir);
}

TEST(beside_ignores_othpatch_itself)
{
    char *const dir = TestDir_Prepare("beside_othpatch");
    M_WriteFile(dir, "othpatch");
    M_WriteFile(dir, "othpatch.exe");
    ARENA arena;
    Arena_Init(&arena, "test", 1 << 16);
    const char *games[APP_GAME_COUNT];
    int32_t count = 0;
    FILE *const out = tmpfile();
    Game_FindBeside(games, &count, &arena, dir, out);
    CHECK_EQ_INT(count, 0);
    fclose(out);
    Arena_Free(&arena);
    Mem_Free(dir);
}

TEST(beside_of_nullptr_finds_nothing)
{
    ARENA arena;
    Arena_Init(&arena, "test", 1 << 16);
    const char *games[APP_GAME_COUNT];
    int32_t count = 0;
    FILE *const out = tmpfile();
    Game_FindBeside(games, &count, &arena, nullptr, out);
    CHECK_EQ_INT(count, 0);
    fclose(out);
    Arena_Free(&arena);
}

TEST(downloaded_finds_game_folders_with_digit_ids)
{
    char *const dir = TestDir_Prepare("downloaded");
    char *const th06 = TestDir_Join(dir, "th06");
    char *const th10 = TestDir_Join(dir, "th10");
    char *const other = TestDir_Join(dir, "other");
    char *const thabc = TestDir_Join(dir, "thabc");
    CHECK(Result_Ignore(File_MakeDir(th06)));
    CHECK(Result_Ignore(File_MakeDir(th10)));
    CHECK(Result_Ignore(File_MakeDir(other)));
    CHECK(Result_Ignore(File_MakeDir(thabc)));
    M_WriteFile(dir, "th99"); // a file, not a folder, is ignored
    ARENA arena;
    Arena_Init(&arena, "test", 1 << 16);
    const char *games[APP_GAME_COUNT];
    int32_t count = 0;
    FILE *const out = tmpfile();
    Game_FindDownloaded(games, &count, &arena, dir, out);
    CHECK_EQ_INT(count, 2);
    CHECK_EQ_STR(games[0], "th06");
    CHECK_EQ_STR(games[1], "th10");
    fclose(out);
    Mem_Free(thabc);
    Mem_Free(other);
    Mem_Free(th10);
    Mem_Free(th06);
    Arena_Free(&arena);
    Mem_Free(dir);
}

TEST(found_games_stay_sorted_and_deduplicated)
{
    const char *games[APP_GAME_COUNT];
    int32_t count = 0;
    FILE *const out = tmpfile();
    Game_AddFound(games, &count, "th10", out);
    Game_AddFound(games, &count, "th06", out);
    Game_AddFound(games, &count, "th08", out);
    Game_AddFound(games, &count, "th06", out);
    CHECK_EQ_INT(count, 3);
    CHECK_EQ_STR(games[0], "th06");
    CHECK_EQ_STR(games[1], "th08");
    CHECK_EQ_STR(games[2], "th10");
    fclose(out);
}

TEST(games_past_the_upper_bound_are_ignored_with_a_warning)
{
    const char *games[APP_GAME_COUNT];
    int32_t count = 0;
    char ids[APP_GAME_COUNT + 1][8];
    FILE *const out = tmpfile();
    for (int32_t i = 0; i < APP_GAME_COUNT + 1; i++) {
        snprintf(ids[i], sizeof(ids[i]), "th%02d", i);
        Game_AddFound(games, &count, ids[i], out);
    }
    CHECK_EQ_INT(count, APP_GAME_COUNT);
    char *const text = M_Read(out);
    CHECK(strstr(text, "th32") != nullptr);
    Mem_Free(text);
}
