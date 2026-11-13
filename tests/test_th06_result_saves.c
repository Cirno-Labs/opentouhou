// SPDX-License-Identifier: GPL-3.0-only

#include "th06/screens/result.h"

#include "test.h"
#include "test_dir.h"

#include "th06/game_manager.h"
#include "th06/replay.h"
#include "th06/replay/format.h"
#include "th06/screens/result/private.h"

#include "core/base/memory.h"
#include "core/io/file.h"

#include <string.h>

GAME_MANAGER g_GameManager;

static uint8_t *M_ScoreBytes(const uint32_t score, size_t *const size)
{
    static SPELL_CARD_RECORD cards[SPELL_CARD_COUNT];
    static CLEAR_RECORD clears[SCORE_SHOT_COUNT];
    static PRACTICE_RECORD practice[SCORE_SHOT_COUNT][SCORE_STAGE_COUNT]
                                   [SCORE_PRACTICE_DIFFICULTY_COUNT];
    static SCORE_LIST lists[SCORE_DIFFICULTY_COUNT][SCORE_SHOT_COUNT];
    const SCORE_FILE empty = {};
    ScoreFile_LoadClears(&empty, clears);
    ScoreFile_LoadPractice(&empty, practice);
    memset(cards, 0, sizeof(cards));
    cards[3].header.magic = 0x4b544143;
    cards[3].attempts = 12;
    memcpy(cards[3].name_bytes, "Card", 5);
    for (int32_t d = 0; d < SCORE_DIFFICULTY_COUNT; d++) {
        for (int32_t s = 0; s < SCORE_SHOT_COUNT; s++) {
            ScoreList_InitDefaults(&lists[d][s], d);
        }
    }
    const HIGH_SCORE entry = { .magic = HIGH_SCORE_MAGIC, .score = score };
    ScoreList_Insert(&lists[1][2], &entry);
    const SCORE_RECORDS records = { cards, clears, practice };
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    ScoreFile_Write(&writer, &empty, lists, &records, 0x21, 0x43);
    uint8_t *const data = Writer_TakeBuffer(&writer, size);
    Result_Ignore(Writer_Close(&writer));
    return data;
}

static uint8_t *M_ReplayBytes(size_t *const size)
{
    REPLAY_FRAME frames[] = { { 0, 0 }, { 5, 0x81 }, { 9999999, 0 } };
    REPLAY_STAGE stage = { .lives = 2, .frames = frames, .frame_count = 3 };
    const REPLAY_FILE file = {
        .shot = 3,
        .difficulty = 1,
        .score = 7654321,
        .stages = { [1] = &stage },
    };
    const REPLAY_SAVE_INFO info = {
        .name = "AB･CD",
        .date = "11/11/26",
        .achieved_frames = 1.0f,
        .expected_frames = 1.0f,
    };
    REPLAY_FORMAT_SAVE prepared;
    CHECK(Result_Log(ReplayFormat_Prepare(&info, &prepared), LOG_LEVEL_ERROR));
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    const uint8_t random[4] = { 70, 1, 2, 3 };
    CHECK(Result_Log(
        ReplayFormat_Write(&writer, &file, &prepared, random),
        LOG_LEVEL_ERROR));
    uint8_t *const data = Writer_TakeBuffer(&writer, size);
    Result_Ignore(Writer_Close(&writer));
    return data;
}

static void M_Save(
    const char *const path, uint8_t *const data, const size_t size)
{
    WRITER writer;
    CHECK(Result_Log(Writer_OpenFile(&writer, path), LOG_LEVEL_ERROR));
    Writer_WriteBytes(&writer, data, size);
    CHECK(Result_Log(Writer_Close(&writer), LOG_LEVEL_ERROR));
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

TEST(open_reads_the_score_file_and_the_replay_slots)
{
    char *const directory = TestDir_Prepare("open");
    char *const score_path = TestDir_Join(directory, "score.dat");
    char *const replay_dir = TestDir_Join(directory, "replay");
    char *const replay_path = TestDir_Join(replay_dir, "th6_02.rpy");
    size_t size;
    uint8_t *const score = M_ScoreBytes(4000000, &size);
    M_Save(score_path, score, size);
    CHECK(Result_Log(File_MakeDir(replay_dir), LOG_LEVEL_ERROR));
    uint8_t *const replay = M_ReplayBytes(&size);
    M_Save(replay_path, replay, size);
    ResultScreen_OpenSaves(directory);
    CHECK_EQ_INT(
        ScoreFile_GetHighScore(ResultSaves_GetScoreFile(), 2, 1), 4000000);
    CHECK(!ResultSaves_GetSlot(0)->valid);
    const SAVED_REPLAY *const slot = ResultSaves_GetSlot(1);
    CHECK(slot->valid);
    CHECK(
        memcmp(
            slot->name,
            "AB\xa5"
            "CD",
            6)
        == 0);
    CHECK_EQ_STR(slot->summary.date, "11/11/26");
    CHECK_EQ_INT(slot->summary.shot, 3);
    CHECK_EQ_INT(slot->summary.score, 7654321);
    ResultScreen_CloseSaves();
    Mem_Free(replay_path);
    Mem_Free(replay_dir);
    Mem_Free(score_path);
    Mem_Free(directory);
}

TEST(open_treats_an_invalid_score_file_as_empty)
{
    char *const directory = TestDir_Prepare("invalid");
    char *const score_path = TestDir_Join(directory, "score.dat");
    uint8_t *const junk = Mem_AllocZ(40);
    junk[5] = 1;
    M_Save(score_path, junk, 40);
    ResultScreen_OpenSaves(directory);
    CHECK(ResultSaves_GetScoreFile()->data == nullptr);
    ResultScreen_CloseSaves();
    Mem_Free(score_path);
    Mem_Free(directory);
}

TEST(put_score_reads_back_at_once_and_writes_after_the_tick)
{
    char *const directory = TestDir_Prepare("put_score");
    char *const score_path = TestDir_Join(directory, "score.dat");
    ResultScreen_OpenSaves(directory);
    size_t size;
    uint8_t *const data = M_ScoreBytes(5000000, &size);
    uint8_t *const copy = Mem_Dup(data, size);
    ResultSaves_PutScoreFile(data, size);
    CHECK_EQ_INT(
        ScoreFile_GetHighScore(ResultSaves_GetScoreFile(), 2, 1), 5000000);
    CHECK(!File_Exists(score_path));
    CHECK(Result_Log(ResultScreen_WriteSaves(), LOG_LEVEL_ERROR));
    CHECK(M_FileEquals(score_path, copy, size));
    SDL_RemovePath(score_path);
    CHECK(Result_Log(ResultScreen_WriteSaves(), LOG_LEVEL_ERROR));
    CHECK(!File_Exists(score_path));
    ResultScreen_CloseSaves();
    Mem_Free(copy);
    Mem_Free(score_path);
    Mem_Free(directory);
}

TEST(put_replay_creates_the_replay_directory)
{
    char *const directory = TestDir_Prepare("put_replay");
    char *const replay_path = TestDir_Join(directory, "replay/th6_15.rpy");
    ResultScreen_OpenSaves(directory);
    size_t size;
    uint8_t *const data = M_ReplayBytes(&size);
    uint8_t *const copy = Mem_Dup(data, size);
    ResultSaves_PutReplay(14, data, size);
    CHECK(ResultSaves_GetSlot(14)->valid);
    CHECK_EQ_INT(ResultSaves_GetSlot(14)->summary.score, 7654321);
    CHECK(Result_Log(ResultScreen_WriteSaves(), LOG_LEVEL_ERROR));
    CHECK(M_FileEquals(replay_path, copy, size));
    ResultScreen_CloseSaves();
    Mem_Free(copy);
    Mem_Free(replay_path);
    Mem_Free(directory);
}

TEST(saves_without_a_user_directory_stay_in_memory)
{
    ResultScreen_OpenSaves(nullptr);
    size_t size;
    uint8_t *const score = M_ScoreBytes(6000000, &size);
    ResultSaves_PutScoreFile(score, size);
    CHECK_EQ_INT(
        ScoreFile_GetHighScore(ResultSaves_GetScoreFile(), 2, 1), 6000000);
    CHECK(Result_Log(ResultScreen_WriteSaves(), LOG_LEVEL_ERROR));
    ResultScreen_CloseSaves();
}

TEST(a_failed_write_is_dropped)
{
    char *const directory = TestDir_Prepare("failed");
    char *const blocker = TestDir_Join(directory, "replay");
    uint8_t *const byte = Mem_AllocZ(1);
    M_Save(blocker, byte, 1);
    ResultScreen_OpenSaves(directory);
    size_t size;
    uint8_t *const replay = M_ReplayBytes(&size);
    ResultSaves_PutReplay(0, replay, size);
    const RESULT result = ResultScreen_WriteSaves();
    CHECK(!IS_OK(result));
    Result_Ignore(result);
    CHECK(Result_Log(ResultScreen_WriteSaves(), LOG_LEVEL_ERROR));
    ResultScreen_CloseSaves();
    Mem_Free(blocker);
    Mem_Free(directory);
}

TEST(clock_starts_with_no_frames)
{
    const RESULT_SCREEN_CLOCK *const clock = ResultSaves_GetClock();
    CHECK_EQ_STR(clock->date, "01/01/00");
    CHECK(clock->achieved_frames == 0.0f);
    CHECK(clock->expected_frames == 0.0f);
}

TEST(set_date_replaces_the_date)
{
    ResultScreen_SetDate("02/03/04");
    CHECK_EQ_STR(ResultSaves_GetClock()->date, "02/03/04");
    ResultScreen_SetDate("01/01/00");
}

static float M_AchievedAt(const float tick_rate)
{
    g_GameManager.mode.playing = true;
    ResultScreen_ResetFrames();
    ResultScreen_CountFrames(tick_rate);
    CHECK(ResultSaves_GetClock()->expected_frames == 60.0f);
    return ResultSaves_GetClock()->achieved_frames;
}

TEST(count_frames_adds_the_share_of_each_band)
{
    CHECK(M_AchievedAt(60.0f) == 60.0f);
    CHECK(M_AchievedAt(54.001f) == 60.0f);
    CHECK(M_AchievedAt(54.0f) == 48.0f);
    CHECK(M_AchievedAt(42.001f) == 48.0f);
    CHECK(M_AchievedAt(42.0f) == 36.0f);
    CHECK(M_AchievedAt(30.001f) == 36.0f);
    CHECK(M_AchievedAt(30.0f) == 30.0f);
    CHECK(M_AchievedAt(0.0f) == 30.0f);
    ResultScreen_ResetFrames();
    g_GameManager.mode.playing = false;
}

TEST(count_frames_adds_up_while_playing)
{
    g_GameManager.mode.playing = true;
    ResultScreen_ResetFrames();
    ResultScreen_CountFrames(60.0f);
    ResultScreen_CountFrames(45.0f);
    CHECK(ResultSaves_GetClock()->expected_frames == 120.0f);
    CHECK(ResultSaves_GetClock()->achieved_frames == 108.0f);
    g_GameManager.mode.playing = false;
    ResultScreen_CountFrames(60.0f);
    CHECK(ResultSaves_GetClock()->expected_frames == 120.0f);
    CHECK(ResultSaves_GetClock()->achieved_frames == 108.0f);
    ResultScreen_ResetFrames();
    CHECK(ResultSaves_GetClock()->expected_frames == 0.0f);
    CHECK(ResultSaves_GetClock()->achieved_frames == 0.0f);
}

TEST(attempt_keeps_the_bytes_after_the_name)
{
    SPELL_CARD_RECORD *const card = &g_GameManager.records.spell_cards[4];
    *card = (SPELL_CARD_RECORD) {};
    memset(card->name_bytes, 0xee, sizeof(card->name_bytes));
    ResultScreen_AddSpellCardAttempt(4, "符", 200000);
    CHECK_EQ_INT(card->name_bytes[0], 0x95);
    CHECK_EQ_INT(card->name_bytes[1], 0x84);
    CHECK_EQ_INT(card->name_bytes[2], 0);
    CHECK_EQ_INT(card->name_bytes[3], 0xee);
    CHECK_EQ_STR(card->name, "符");
    CHECK_EQ_INT(card->checksum, (0x95 + 0x84) & 0xff);
    CHECK_EQ_INT(card->bonus, 200000);
    CHECK_EQ_INT(card->attempts, 1);
}

TEST(attempt_resets_the_counts_when_the_name_sum_changes)
{
    SPELL_CARD_RECORD *const card = &g_GameManager.records.spell_cards[5];
    *card = (SPELL_CARD_RECORD) {
        .checksum = 'A' + 'B',
        .attempts = 7,
        .captures = 3,
        .history = { 1, 2, 3, 0, 0 },
    };
    ResultScreen_AddSpellCardAttempt(5, "BA", 10);
    CHECK_EQ_INT(card->attempts, 8);
    CHECK_EQ_INT(card->captures, 3);
    ResultScreen_AddSpellCardAttempt(5, "ABC", 20);
    CHECK_EQ_INT(card->attempts, 1);
    CHECK_EQ_INT(card->captures, 0);
    CHECK_EQ_INT(card->history[0], 1);
    CHECK_EQ_INT(card->checksum, ('A' + 'B' + 'C') & 0xff);
    CHECK_EQ_INT(card->bonus, 20);
}

TEST(attempts_stop_at_9999_but_captures_do_not)
{
    SPELL_CARD_RECORD *const card = &g_GameManager.records.spell_cards[6];
    *card = (SPELL_CARD_RECORD) {
        .checksum = 'X',
        .attempts = 9999,
        .captures = 9999,
    };
    ResultScreen_AddSpellCardAttempt(6, "X", 1);
    CHECK_EQ_INT(card->attempts, 9999);
    ResultScreen_AddSpellCardCapture(6, 2);
    CHECK_EQ_INT(card->captures, 10000);
}

TEST(capture_puts_the_shot_first_in_the_history)
{
    SPELL_CARD_RECORD *const card = &g_GameManager.records.spell_cards[7];
    *card = (SPELL_CARD_RECORD) { .history = { 1, 2, 3, 0, 1 } };
    ResultScreen_AddSpellCardCapture(7, 3);
    CHECK_EQ_INT(card->history[0], 3);
    CHECK_EQ_INT(card->history[1], 1);
    CHECK_EQ_INT(card->history[2], 2);
    CHECK_EQ_INT(card->history[3], 3);
    CHECK_EQ_INT(card->history[4], 0);
    CHECK_EQ_INT(card->captures, 1);
}

TEST(load_game_records_reads_the_saved_score_file)
{
    ResultScreen_OpenSaves(nullptr);
    size_t size;
    uint8_t *const score = M_ScoreBytes(3000000, &size);
    ResultSaves_PutScoreFile(score, size);
    GAME_MANAGER *const gm = &g_GameManager;
    gm->records.spell_cards[2].bonus = 77;
    uint32_t high = 0;
    ResultScreen_LoadGameRecords(
        &high, gm->records.spell_cards, gm->records.clears,
        gm->records.practice, 2, 1);
    CHECK_EQ_INT(high, 3000000);
    CHECK_EQ_INT(gm->records.spell_cards[2].bonus, 77);
    CHECK_EQ_INT(gm->records.spell_cards[3].attempts, 12);
    CHECK_EQ_INT(gm->records.clears[1].with_continues[2], 1);
    ResultScreen_LoadGameRecords(
        &high, gm->records.spell_cards, gm->records.clears,
        gm->records.practice, 0, 1);
    CHECK_EQ_INT(high, 1000000);
    ResultScreen_CloseSaves();
}

TEST(list_gives_the_slots_then_the_user_replays)
{
    char *const directory = TestDir_Prepare("list");
    char *const replay_dir = TestDir_Join(directory, "replay");
    CHECK(Result_Log(File_MakeDir(replay_dir), LOG_LEVEL_ERROR));
    const char *const names[] = {
        "th6_03.rpy",     "th6_01.rpy",   "th6_udB000.rpy",
        "TH6_UD0001.rpy", "th6_ud12.rpy", "other.rpy",
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char *const path = TestDir_Join(replay_dir, names[i]);
        size_t size;
        uint8_t *const replay = M_ReplayBytes(&size);
        M_Save(path, replay, size);
        Mem_Free(path);
    }
    char *const junk_path = TestDir_Join(replay_dir, "th6_ud0002.rpy");
    M_Save(junk_path, Mem_AllocZ(8), 8);
    Mem_Free(junk_path);
    ResultScreen_OpenSaves(directory);
    static SAVED_REPLAY list[SAVED_REPLAY_COUNT];
    CHECK_EQ_INT(ResultScreen_ListReplays(list), 4);
    CHECK_EQ_STR(list[0].label, "No.01");
    CHECK_EQ_STR(list[1].label, "No.03");
    CHECK_EQ_STR(list[2].label, "User ");
    CHECK_EQ_STR(list[3].label, "User ");
    const char *const ends[] = {
        "replay/th6_01.rpy",
        "replay/th6_03.rpy",
        "replay/TH6_UD0001.rpy",
        "replay/th6_udB000.rpy",
    };
    for (int32_t i = 0; i < 4; i++) {
        const size_t length = strlen(list[i].path);
        const size_t end = strlen(ends[i]);
        CHECK(
            length > end && strcmp(list[i].path + length - end, ends[i]) == 0);
        CHECK(strncmp(list[i].path, directory, strlen(directory)) == 0);
        CHECK_EQ_INT(list[i].summary.lives, 2);
        CHECK(list[i].summary.stages[1]);
        CHECK_EQ_INT(list[i].summary.difficulty, 1);
    }
    ResultScreen_CloseSaves();
    Mem_Free(replay_dir);
    Mem_Free(directory);
}

TEST(list_follows_a_saved_slot)
{
    ResultScreen_OpenSaves(nullptr);
    static SAVED_REPLAY list[SAVED_REPLAY_COUNT];
    CHECK_EQ_INT(ResultScreen_ListReplays(list), 0);
    size_t size;
    uint8_t *const replay = M_ReplayBytes(&size);
    ResultSaves_PutReplay(4, replay, size);
    CHECK_EQ_INT(ResultScreen_ListReplays(list), 1);
    CHECK_EQ_STR(list[0].label, "No.05");
    CHECK_EQ_STR(list[0].path, "replay/th6_05.rpy");
    ResultScreen_CloseSaves();
}

TEST(a_scan_finds_a_new_user_replay_and_drops_a_deleted_one)
{
    char *const directory = TestDir_Prepare("scan");
    char *const replay_dir = TestDir_Join(directory, "replay");
    CHECK(Result_Log(File_MakeDir(replay_dir), LOG_LEVEL_ERROR));
    char *const first = TestDir_Join(replay_dir, "th6_ud0001.rpy");
    char *const second = TestDir_Join(replay_dir, "th6_ud0002.rpy");
    size_t size;
    uint8_t *const first_replay = M_ReplayBytes(&size);
    M_Save(first, first_replay, size);
    ResultScreen_OpenSaves(directory);
    static SAVED_REPLAY list[SAVED_REPLAY_COUNT];
    CHECK_EQ_INT(ResultScreen_ListReplays(list), 1);
    CHECK(Replay_FindFile(list[0].path, &size) != nullptr);
    char kept[SAVED_REPLAY_PATH_SIZE];
    memcpy(kept, list[0].path, sizeof(kept));

    SDL_RemovePath(first);
    uint8_t *const second_replay = M_ReplayBytes(&size);
    M_Save(second, second_replay, size);
    const int32_t scan = ResultScreen_GetReplayScan();
    ResultScreen_ScanReplays();
    CHECK_EQ_INT(ResultScreen_GetReplayScan(), scan);
    ResultScreen_RequestReplayScan();
    ResultScreen_ScanReplays();
    CHECK(ResultScreen_GetReplayScan() != scan);
    CHECK_EQ_INT(ResultScreen_ListReplays(list), 1);
    const size_t length = strlen(list[0].path);
    CHECK(strcmp(list[0].path + length - 14, "th6_ud0002.rpy") == 0);
    CHECK(Replay_FindFile(list[0].path, &size) != nullptr);
    CHECK(Replay_FindFile(kept, &size) == nullptr);
    ResultScreen_CloseSaves();
    CHECK(Replay_FindFile(list[0].path, &size) == nullptr);
    Mem_Free(second);
    Mem_Free(first);
    Mem_Free(replay_dir);
    Mem_Free(directory);
}

TEST(a_saved_slot_is_kept_for_playback)
{
    ResultScreen_OpenSaves(nullptr);
    size_t size;
    uint8_t *const data = M_ReplayBytes(&size);
    uint8_t *const copy = Mem_Dup(data, size);
    ResultSaves_PutReplay(2, data, size);
    size_t kept_size = 0;
    const uint8_t *const kept =
        Replay_FindFile("replay/th6_03.rpy", &kept_size);
    CHECK(kept != nullptr && kept_size == size);
    CHECK(kept != nullptr && memcmp(kept, copy, size) == 0);
    ResultScreen_CloseSaves();
    Mem_Free(copy);
}
