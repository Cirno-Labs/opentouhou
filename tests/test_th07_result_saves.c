#include "th07/screens/result/private.h"
#include "th07/screens/result/records.h"

#include "test.h"
#include "test_dir.h"

#include "th07/game.h"
#include "th07/game_manager.h"
#include "th07/replay.h"

#include "shell/null.h"
#include "core/base/memory.h"
#include "core/io/file.h"

#include <string.h>

RNG g_Rng;

static uint8_t *M_TakeBuffer(WRITER *const writer, size_t *const size)
{
    uint8_t *const data = Writer_TakeBuffer(writer, size);
    Result_Ignore(Writer_Close(writer));
    return data;
}

// Returns a score file whose list of shot 2 on Hard holds score, and whose
// clears of shot 1 show stage 4.
static uint8_t *M_ScoreBytes(const uint32_t score, size_t *const size)
{
    static SPELL_CARD_RECORD cards[SPELL_CARD_COUNT];
    static CLEAR_RECORD clears[SCORE_SHOT_COUNT];
    static PRACTICE_RECORD practice[SCORE_SHOT_COUNT][SCORE_STAGE_COUNT]
                                   [SCORE_PRACTICE_DIFFICULTY_COUNT];
    static SCORE_LIST lists[SCORE_DIFFICULTY_COUNT][SCORE_SHOT_COUNT];
    static const LAST_NAME_RECORD last_name = {
        .header = { .magic = 0x4d4e534c, .size = 0x18, .size2 = 0x18 },
    };
    static PLAY_STATS_RECORD play_stats;
    const SCORE_FILE empty = {};
    ScoreFile_LoadClears(&empty, clears);
    ScoreFile_LoadPractice(&empty, practice);
    memset(cards, 0, sizeof(cards));
    clears[1].with_continues[2] = 4;
    for (int32_t d = 0; d < SCORE_DIFFICULTY_COUNT; d++) {
        for (int32_t s = 0; s < SCORE_SHOT_COUNT; s++) {
            ScoreList_InitDefaults(&lists[d][s], d);
        }
    }
    const HIGH_SCORE entry = {
        .magic = HIGH_SCORE_MAGIC,
        .score = score,
        .continues_used = 3,
    };
    ScoreList_Insert(&lists[2][2], &entry);
    play_stats.header.magic = 0x54534c50;
    play_stats.header.size = 0x160;
    play_stats.header.version = 1;
    play_stats.play_hours = 11;
    const SCORE_RECORDS records = {
        cards, clears, practice, &last_name, &play_stats,
    };
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    ScoreFile_Write(&writer, &empty, lists, &records, 0x21, 0x43);
    return M_TakeBuffer(&writer, size);
}

static uint8_t *M_ReplayBytes(const uint32_t score, size_t *const size)
{
    REPLAY_FRAME frames[] = { { 0, 0 }, { 5, 0 }, { 0, 0 } };
    REPLAY_STAGE stage = { .lives = 2, .frames = frames, .frame_count = 3 };
    uint8_t fps[] = { 60, 60 };
    const REPLAY_FILE file = {
        .shot = 3,
        .difficulty = 1,
        .date = "11/16",
        .name = "TEST",
        .score = score,
        .stages = { [1] = &stage },
        .fps = { [1] = fps },
        .fps_counts = { [1] = 2 },
    };
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    const uint8_t random[3] = { 70, 1, 2 };
    CHECK(Result_Log(Replay_Write(&file, &writer, random), LOG_LEVEL_ERROR));
    return M_TakeBuffer(&writer, size);
}

static void M_Save(
    const char *const path, uint8_t *const data, const size_t size)
{
    TestDir_Write(path, data, size);
    Allocator_Free(&g_HeapAllocator, data, size);
}

static bool M_FileEquals(
    const char *const path, const uint8_t *const data, const size_t size)
{
    READER reader;
    if (!Result_Log(Reader_OpenFile(&reader, path), LOG_LEVEL_ERROR)) {
        return false;
    }
    bool equal = Reader_Size(&reader) == size;
    for (size_t i = 0; equal && i < size; i++) {
        equal = Reader_ReadU8(&reader) == data[i];
    }
    Reader_Close(&reader);
    return equal;
}

static void M_SetUp(const char *const user_dir)
{
    GameManager_Create(&g_GameManager);
    g_GameManager.run.shot_index = 2;
    g_GameManager.run.difficulty = 2;
    Rng_SetSeed(&g_Rng, 0x1234);
    CHECK(Result_Ignore(NullShell_OpenFiles(nullptr, user_dir)));
    ResultScreen_OpenSaves();
    ResultScreen_LoadPlayStats();
}

static void M_TearDown(void)
{
    ResultScreen_CloseSaves();
    Replay_ClearFiles();
    NullShell_CloseFiles();
}

// Opens a user directory whose score.dat holds a high score of score.
static char *M_OpenWithScore(const char *const name, const uint32_t score)
{
    char *const directory = TestDir_Prepare(name);
    char *const path = TestDir_Join(directory, "score.dat");
    size_t size;
    uint8_t *const data = M_ScoreBytes(score, &size);
    M_Save(path, data, size);
    Mem_Free(path);
    M_SetUp(directory);
    return directory;
}

static SPELL_CARD_RECORD *M_Card(const int32_t index)
{
    return &g_GameManager.records.spell_cards[index];
}

TEST(open_reads_the_game_records_of_the_score_file)
{
    char *const directory = M_OpenWithScore("open", 4000000);
    ResultScreen_LoadGameRecords();
    CHECK_EQ_INT(g_GameManager.score.high, 4000000);
    CHECK_EQ_INT(g_GameManager.score.high_continues, 3);
    CHECK_EQ_INT(g_GameManager.records.clears[1].with_continues[2], 4);
    CHECK_EQ_INT(g_GameManager.records.play_stats.play_hours, 11);
    M_TearDown();
    Mem_Free(directory);
}

TEST(open_treats_an_invalid_score_file_as_empty)
{
    char *const directory = TestDir_Prepare("invalid");
    char *const path = TestDir_Join(directory, "score.dat");
    TestDir_Write(path, "not a score file at all, too short?", 36);
    M_SetUp(directory);
    ResultScreen_LoadGameRecords();
    CHECK_EQ_INT(g_GameManager.score.high, SCORE_DEFAULT_HIGH_SCORE);
    CHECK_EQ_INT(g_GameManager.records.play_stats.header.size, 0x160);
    M_TearDown();
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(save_score_draws_the_key_and_random_byte)
{
    M_SetUp(nullptr);
    RNG expected = g_Rng;
    const uint8_t key = (uint8_t)Rng_U16InRange(&expected, 256);
    const uint8_t random = (uint8_t)Rng_U16InRange(&expected, 256);
    ResultScreen_SaveScore();
    CHECK(memcmp(&g_Rng, &expected, sizeof(expected)) == 0);
    const SCORE_FILE *const file = ResultSaves_GetScoreFile();
    CHECK_EQ_INT(file->data[1], key);
    CHECK_EQ_INT(file->data[6], random);
    M_TearDown();
}

TEST(save_score_reloads_the_records_and_keeps_the_play_stats)
{
    char *const directory = M_OpenWithScore("reload", 4000000);
    g_GameManager.records.clears[1].with_continues[2] = 6;
    g_GameManager.records.practice[0][0][0].score = 99;
    g_GameManager.records.play_stats.play_hours = 12;
    ResultScreen_SaveScore();
    CHECK_EQ_INT(g_GameManager.records.clears[1].with_continues[2], 4);
    CHECK_EQ_INT(g_GameManager.records.practice[0][0][0].score, 0);
    CHECK_EQ_INT(
        ScoreFile_GetHighScore(
            ResultSaves_GetScoreFile(), 2, 2,
            &g_GameManager.score.high_continues),
        4000000);
    g_GameManager.records.play_stats.play_hours = 0;
    ResultScreen_LoadPlayStats();
    CHECK_EQ_INT(g_GameManager.records.play_stats.play_hours, 12);
    M_TearDown();
    Mem_Free(directory);
}

TEST(a_saved_score_file_is_written_after_the_tick)
{
    char *const directory = TestDir_Prepare("write");
    char *const path = TestDir_Join(directory, "score.dat");
    M_SetUp(directory);
    g_GameManager.records.play_stats.play_hours = 3;
    ResultScreen_SaveScore();
    CHECK(!File_Exists(path));
    CHECK(Result_Log(ResultScreen_WriteSaves(), LOG_LEVEL_ERROR));
    CHECK(File_Exists(path));
    M_TearDown();
    M_SetUp(directory);
    CHECK_EQ_INT(g_GameManager.records.play_stats.play_hours, 3);
    M_TearDown();
    Mem_Free(path);
    Mem_Free(directory);
}

TEST(saves_without_a_user_directory_stay_in_memory_and_fail_to_write)
{
    M_SetUp(nullptr);
    g_GameManager.records.play_stats.play_hours = 5;
    ResultScreen_SaveScore();
    const RESULT result = ResultScreen_WriteSaves();
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    g_GameManager.records.play_stats.play_hours = 0;
    ResultScreen_LoadPlayStats();
    CHECK_EQ_INT(g_GameManager.records.play_stats.play_hours, 5);
    M_TearDown();
}

TEST(put_replay_file_replaces_a_listed_replay_and_writes_it)
{
    char *const directory = TestDir_Prepare("resave");
    char *const replay_dir = TestDir_Join(directory, "replay");
    char *const replay_path = TestDir_Join(replay_dir, "th7_03.rpy");
    CHECK(Result_Log(File_MakeDir(replay_dir), LOG_LEVEL_ERROR));
    size_t size;
    uint8_t *const first = M_ReplayBytes(111, &size);
    M_Save(replay_path, first, size);
    M_SetUp(directory);
    SAVED_REPLAY listed[SAVED_REPLAY_COUNT];
    CHECK_EQ_INT(ResultScreen_ListReplays(listed), 1);
    char path[SAVED_REPLAY_PATH_SIZE];
    memcpy(path, listed[0].path, sizeof(path));

    uint8_t *const data = M_ReplayBytes(222, &size);
    uint8_t *const copy = Mem_Dup(data, size);
    ResultScreen_PutReplayFile(path, data, size);
    CHECK_EQ_INT(ResultScreen_ListReplays(listed), 1);
    CHECK_EQ_INT(listed[0].summary.score, 222);
    CHECK_EQ_STR(listed[0].label, "No.03");
    size_t kept_size;
    const uint8_t *const kept = Replay_FindFile(path, &kept_size);
    CHECK(kept != nullptr && kept_size == size);
    CHECK(memcmp(kept, copy, size) == 0);
    CHECK(!M_FileEquals(replay_path, copy, size));
    CHECK(Result_Log(ResultScreen_WriteSaves(), LOG_LEVEL_ERROR));
    CHECK(M_FileEquals(replay_path, copy, size));
    M_TearDown();
    Mem_Free(copy);
    Mem_Free(replay_path);
    Mem_Free(replay_dir);
    Mem_Free(directory);
}

TEST(put_replay_file_keeps_a_path_outside_the_replay_directory_in_memory)
{
    char *const directory = TestDir_Prepare("outside");
    char *const user_dir = TestDir_Join(directory, "user");
    CHECK(Result_Log(File_MakeDir(user_dir), LOG_LEVEL_ERROR));
    M_SetUp(user_dir);
    const char *const paths[] = { "th7_01.rpy", "scratch/th7_01.rpy" };
    for (int32_t i = 0; i < 2; i++) {
        size_t size;
        uint8_t *const data = M_ReplayBytes(5, &size);
        ResultScreen_PutReplayFile(paths[i], data, size);
        size_t kept_size;
        CHECK(Replay_FindFile(paths[i], &kept_size) != nullptr);
    }
    CHECK(Result_Log(ResultScreen_WriteSaves(), LOG_LEVEL_ERROR));
    FILE_LIST list;
    CHECK(Result_Log(
        File_ListDir(user_dir, &g_HeapAllocator, &list), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(list.count, 0);
    File_FreeList(&list);
    M_TearDown();
    Mem_Free(user_dir);
    Mem_Free(directory);
}

TEST(attempt_stores_the_name_and_counts_for_the_shot_and_every_shot)
{
    M_SetUp(nullptr);
    ResultScreen_LoadGameRecords();
    SPELL_CARD_RECORD *const card = M_Card(7);
    memset(card->name, 0xcc, sizeof(card->name));
    ResultScreen_AddSpellCardAttempt(7, "AB");
    CHECK_EQ_STR((const char *)card->name, "AB");
    CHECK_EQ_INT(card->name[3], 0xcc);
    CHECK_EQ_INT(card->attempts[2], 1);
    CHECK_EQ_INT(card->attempts[SCORE_ALL_SHOTS], 1);
    CHECK_EQ_INT(card->checksum, (uint8_t)('A' + 'B' + 2));
    ResultScreen_AddSpellCardAttempt(7, "AB");
    CHECK_EQ_INT(card->attempts[2], 2);
    int32_t captures;
    int32_t attempts;
    ResultScreen_GetSpellCardRecord(7, &captures, &attempts);
    CHECK_EQ_INT(captures, 0);
    CHECK_EQ_INT(attempts, 2);
    M_TearDown();
}

TEST(attempt_resets_the_counts_when_the_checksum_does_not_match)
{
    M_SetUp(nullptr);
    ResultScreen_LoadGameRecords();
    SPELL_CARD_RECORD *const card = M_Card(3);
    card->attempts[0] = 40;
    card->captures[6] = 20;
    card->max_bonus[1] = 1000;
    card->checksum = 0;
    ResultScreen_AddSpellCardAttempt(3, "X");
    CHECK_EQ_INT(card->attempts[0], 0);
    CHECK_EQ_INT(card->captures[6], 0);
    CHECK_EQ_INT(card->max_bonus[1], 0);
    CHECK_EQ_INT(card->attempts[6], 1);
    M_TearDown();
}

TEST(counts_stop_at_9999)
{
    M_SetUp(nullptr);
    ResultScreen_LoadGameRecords();
    SPELL_CARD_RECORD *const card = M_Card(0);
    ResultScreen_AddSpellCardAttempt(0, "A");
    card->attempts[2] = 9999;
    card->captures[6] = 9999;
    card->checksum = (uint8_t)(card->checksum + 9998 + 9999);
    ResultScreen_AddSpellCardAttempt(0, "A");
    ResultScreen_AddSpellCardCapture(0, 10);
    CHECK_EQ_INT(card->attempts[2], 9999);
    CHECK_EQ_INT(card->attempts[6], 2);
    CHECK_EQ_INT(card->captures[6], 9999);
    CHECK_EQ_INT(card->captures[2], 1);
    M_TearDown();
}

TEST(capture_keeps_the_best_bonus_compared_unsigned)
{
    M_SetUp(nullptr);
    ResultScreen_LoadGameRecords();
    SPELL_CARD_RECORD *const card = M_Card(9);
    ResultScreen_AddSpellCardCapture(9, 500);
    ResultScreen_AddSpellCardCapture(9, 300);
    CHECK_EQ_INT(card->max_bonus[2], 500);
    CHECK_EQ_INT(card->max_bonus[SCORE_ALL_SHOTS], 500);
    ResultScreen_AddSpellCardCapture(9, -1);
    CHECK_EQ_INT(card->max_bonus[2], 0xffffffff);
    CHECK_EQ_INT(card->captures[2], 3);
    M_TearDown();
}

TEST(load_game_records_empties_the_spell_cards)
{
    M_SetUp(nullptr);
    M_Card(5)->captures[6] = 7;
    ResultScreen_LoadGameRecords();
    CHECK_EQ_INT(M_Card(5)->captures[6], 0);
    CHECK_EQ_INT(M_Card(5)->header.magic, SPELL_CARD_MAGIC);
    CHECK_EQ_INT(M_Card(5)->header.size, 0x78);
    CHECK_EQ_INT(M_Card(5)->index, 5);
    M_TearDown();
}

TEST(load_title_records_keeps_the_spell_cards)
{
    M_SetUp(nullptr);
    M_Card(5)->captures[6] = 7;
    g_GameManager.records.clears[0].no_continues[0] = 9;
    ResultScreen_LoadTitleRecords();
    CHECK_EQ_INT(M_Card(5)->captures[6], 7);
    CHECK_EQ_INT(g_GameManager.records.clears[0].no_continues[0], 1);
    M_TearDown();
}
