#include "othpatch/game.h"

#include "core/base/arena.h"
#include "core/base/memory.h"
#include "core/base/utils.h"
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

// Holds a discovery test: its directory, or nullptr, the arena of the
// found strings, the found games and the stream for the warnings.
typedef struct {
    char *dir;
    ARENA arena;
    GAME_LIST games;
    FILE *out;
} M_FIND;

static void M_FindInit(M_FIND *const find, const char *const name)
{
    find->dir = name != nullptr ? TestDir_Prepare(name) : nullptr;
    Arena_Init(&find->arena, "test", 1 << 16);
    find->games = (GAME_LIST) {};
    find->out = tmpfile();
}

static void M_FindFree(M_FIND *const find)
{
    fclose(find->out);
    Arena_Free(&find->arena);
    Mem_Free(find->dir);
}

TEST(beside_finds_every_known_form)
{
    M_FIND find;
    M_FindInit(&find, "beside_forms");
    M_WriteFile(find.dir, "oth06");
    M_WriteFile(find.dir, "oth99.exe");
    M_WriteFile(find.dir, "oth08.AppImage");
    M_WriteFile(find.dir, "oth09.app");
    Game_FindBeside(&find.games, &find.arena, find.dir, find.out);
    CHECK_EQ_INT(find.games.count, 4);
    CHECK_EQ_STR(find.games.items[0], "th06");
    CHECK_EQ_STR(find.games.items[1], "th08");
    CHECK_EQ_STR(find.games.items[2], "th09");
    CHECK_EQ_STR(find.games.items[3], "th99");
    M_FindFree(&find);
}

TEST(beside_ignores_a_non_matching_name)
{
    M_FIND find;
    M_FindInit(&find, "beside_non_matching");
    M_WriteFile(find.dir, "the_game");
    M_WriteFile(find.dir, "oth_foo");
    M_WriteFile(find.dir, "oth06x.exe");
    Game_FindBeside(&find.games, &find.arena, find.dir, find.out);
    CHECK_EQ_INT(find.games.count, 0);
    M_FindFree(&find);
}

TEST(beside_ignores_othpatch_itself)
{
    M_FIND find;
    M_FindInit(&find, "beside_othpatch");
    M_WriteFile(find.dir, "othpatch");
    M_WriteFile(find.dir, "othpatch.exe");
    Game_FindBeside(&find.games, &find.arena, find.dir, find.out);
    CHECK_EQ_INT(find.games.count, 0);
    M_FindFree(&find);
}

TEST(beside_of_nullptr_finds_nothing)
{
    M_FIND find;
    M_FindInit(&find, nullptr);
    Game_FindBeside(&find.games, &find.arena, nullptr, find.out);
    CHECK_EQ_INT(find.games.count, 0);
    M_FindFree(&find);
}

TEST(downloaded_finds_game_folders_with_digit_ids)
{
    M_FIND find;
    M_FindInit(&find, "downloaded");
    static const char *const dirs[] = { "th06", "th10", "other", "thabc" };
    for (size_t i = 0; i < ARRAY_SIZE(dirs); i++) {
        char *const path = TestDir_Join(find.dir, dirs[i]);
        CHECK(Result_Ignore(File_MakeDir(path)));
        Mem_Free(path);
    }
    M_WriteFile(find.dir, "th99"); // a file, not a folder, is ignored
    Game_FindDownloaded(&find.games, &find.arena, find.dir, find.out);
    CHECK_EQ_INT(find.games.count, 2);
    CHECK_EQ_STR(find.games.items[0], "th06");
    CHECK_EQ_STR(find.games.items[1], "th10");
    M_FindFree(&find);
}

TEST(found_games_stay_sorted_and_deduplicated)
{
    GAME_LIST games = {};
    FILE *const out = tmpfile();
    Game_AddFound(&games, "th10", out);
    Game_AddFound(&games, "th06", out);
    Game_AddFound(&games, "th08", out);
    Game_AddFound(&games, "th06", out);
    CHECK_EQ_INT(games.count, 3);
    CHECK_EQ_STR(games.items[0], "th06");
    CHECK_EQ_STR(games.items[1], "th08");
    CHECK_EQ_STR(games.items[2], "th10");
    fclose(out);
}

TEST(games_past_the_upper_bound_are_ignored_with_a_warning)
{
    GAME_LIST games = {};
    char ids[GAME_MAX_COUNT + 1][8];
    FILE *const out = tmpfile();
    for (int32_t i = 0; i < GAME_MAX_COUNT + 1; i++) {
        snprintf(ids[i], sizeof(ids[i]), "th%02d", i);
        Game_AddFound(&games, ids[i], out);
    }
    CHECK_EQ_INT(games.count, GAME_MAX_COUNT);
    char *const text = M_Read(out);
    CHECK(strstr(text, "th32") != nullptr);
    Mem_Free(text);
}
