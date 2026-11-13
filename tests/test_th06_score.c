// SPDX-License-Identifier: GPL-3.0-only

#include "th06/screens/result/score.h"

#include "test.h"

#include "core/base/memory.h"

#include <string.h>

typedef struct {
    uint8_t data[4096];
    int32_t size;
} M_BYTES;

static SPELL_CARD_RECORD m_Cards[SPELL_CARD_COUNT];
static CLEAR_RECORD m_Clears[SCORE_SHOT_COUNT];
static PRACTICE_RECORD m_Practice[SCORE_SHOT_COUNT][SCORE_STAGE_COUNT]
                                 [SCORE_PRACTICE_DIFFICULTY_COUNT];
static SCORE_LIST m_Lists[SCORE_DIFFICULTY_COUNT][SCORE_SHOT_COUNT];

static void M_Put8(M_BYTES *const bytes, const uint32_t value)
{
    bytes->data[bytes->size++] = (uint8_t)value;
}

static void M_Put16(M_BYTES *const bytes, const uint32_t value)
{
    M_Put8(bytes, value);
    M_Put8(bytes, value >> 8);
}

static void M_Put32(M_BYTES *const bytes, const uint32_t value)
{
    M_Put16(bytes, value);
    M_Put16(bytes, value >> 16);
}

static uint32_t M_Get16(const uint8_t *const data, const int32_t at)
{
    return (uint32_t)data[at] | (uint32_t)data[at + 1] << 8;
}

static uint32_t M_Get32(const uint8_t *const data, const int32_t at)
{
    return M_Get16(data, at) | M_Get16(data, at + 2) << 16;
}

static void M_PutChunkHeader(
    M_BYTES *const bytes, const char *const magic, const uint32_t size,
    const uint32_t version)
{
    for (int32_t i = 0; i < 4; i++) {
        M_Put8(bytes, (uint8_t)magic[i]);
    }
    M_Put16(bytes, size);
    M_Put16(bytes, size);
    M_Put8(bytes, version);
    M_Put8(bytes, 0);
    M_Put16(bytes, 0);
}

static void M_PutFileHeader(M_BYTES *const bytes)
{
    *bytes = (M_BYTES) {};
    for (int32_t i = 0; i < 20; i++) {
        M_Put8(bytes, 0);
    }
    M_PutChunkHeader(bytes, "TH6K", 12, 0x10);
}

static void M_PutHighScore(
    M_BYTES *const bytes, const uint32_t score, const int32_t shot,
    const int32_t difficulty, const char *const name)
{
    M_PutChunkHeader(bytes, "HSCR", 0x1c, 0x10);
    M_Put32(bytes, score);
    M_Put8(bytes, (uint32_t)shot);
    M_Put8(bytes, (uint32_t)difficulty);
    M_Put8(bytes, 3);
    for (int32_t i = 0; i < 9; i++) {
        M_Put8(bytes, (uint8_t)name[i]);
    }
}

static void M_PutPractice(
    M_BYTES *const bytes, const uint32_t score, const int32_t shot,
    const int32_t stage, const int32_t difficulty)
{
    M_PutChunkHeader(bytes, "PSCR", 0x14, 0x10);
    M_Put32(bytes, score);
    M_Put8(bytes, (uint32_t)shot);
    M_Put8(bytes, (uint32_t)difficulty);
    M_Put8(bytes, (uint32_t)stage);
    M_Put8(bytes, 0);
}

static void M_PutClear(M_BYTES *const bytes, const int32_t shot)
{
    M_PutChunkHeader(bytes, "CLRD", 0x18, 0x10);
    for (int32_t i = 0; i < 10; i++) {
        M_Put8(bytes, (uint32_t)(10 + i));
    }
    M_Put8(bytes, (uint32_t)shot);
    M_Put8(bytes, 0);
}

static void M_PutSpellCard(
    M_BYTES *const bytes, const int32_t index, const char *const name)
{
    M_PutChunkHeader(bytes, "CATK", 0x40, 0x10);
    M_Put32(bytes, 300000);
    M_Put16(bytes, (uint32_t)index);
    M_Put8(bytes, 0x42);
    for (int32_t i = 0; i < 5; i++) {
        M_Put8(bytes, (uint32_t)i);
    }
    const size_t length = strlen(name);
    for (size_t i = 0; i < 36; i++) {
        M_Put8(bytes, i < length ? (uint8_t)name[i] : 0xcc);
        if (i == length) {
            bytes->data[bytes->size - 1] = 0;
        }
    }
    M_Put16(bytes, 7);
    M_Put16(bytes, 3);
}

// Sets the header fields, with file_size as the stored size, and encrypts
// bytes the way the original's SaveScoreDat does.
static void M_SealAs(
    M_BYTES *const bytes, const uint8_t key, const int32_t file_size)
{
    uint8_t *const data = bytes->data;
    data[1] = key;
    data[4] = 0x10;
    data[8] = 20;
    const int32_t size = bytes->size;
    data[16] = (uint8_t)file_size;
    data[17] = (uint8_t)(file_size >> 8);
    uint16_t checksum = 0;
    for (int32_t i = 4; i < size; i++) {
        checksum = (uint16_t)(checksum + data[i]);
    }
    data[2] = (uint8_t)checksum;
    data[3] = (uint8_t)(checksum >> 8);
    uint8_t *p = data + 1;
    uint8_t rolling = *p;
    for (int32_t m = size - 2; m > 0; m--, p++) {
        const uint8_t previous = p[1];
        rolling = (uint8_t)(((rolling & 0xe0) >> 5) | ((rolling & 0x1f) << 3));
        p[1] ^= rolling;
        rolling = (uint8_t)(rolling + previous);
    }
}

static void M_Seal(M_BYTES *const bytes, const uint8_t key)
{
    M_SealAs(bytes, key, bytes->size);
}

// Decrypts bytes the way the original's LoadScoreDat does.
static void M_Open(uint8_t *const data, const int32_t size)
{
    uint8_t key = 0;
    uint8_t *p = data + 1;
    for (int32_t remaining = size - 2; remaining > 0; remaining--, p++) {
        key = (uint8_t)(key + *p);
        key = (uint8_t)(((key & 0xe0) >> 5) | ((key & 0x1f) << 3));
        p[1] ^= key;
    }
}

static SCORE_FILE M_Read(const M_BYTES *const bytes)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes->data, (size_t)bytes->size);
    SCORE_FILE file;
    CHECK(Result_Log(ScoreFile_Read(&reader, &file), LOG_LEVEL_ERROR));
    return file;
}

static bool M_ReadFails(const M_BYTES *const bytes)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes->data, (size_t)bytes->size);
    SCORE_FILE file;
    const RESULT result = ScoreFile_Read(&reader, &file);
    const bool failed = !IS_OK(result);
    Result_Ignore(result);
    CHECK(file.data == nullptr);
    ScoreFile_Free(&file);
    return failed;
}

static SCORE_RECORDS M_Records(void)
{
    return (SCORE_RECORDS) {
        .spell_cards = m_Cards,
        .clears = m_Clears,
        .practice = m_Practice,
    };
}

static void M_ResetRecords(void)
{
    memset(m_Cards, 0, sizeof(m_Cards));
    const SCORE_FILE empty = {};
    ScoreFile_LoadClears(&empty, m_Clears);
    ScoreFile_LoadPractice(&empty, m_Practice);
    for (int32_t d = 0; d < SCORE_DIFFICULTY_COUNT; d++) {
        for (int32_t s = 0; s < SCORE_SHOT_COUNT; s++) {
            ScoreList_InitDefaults(&m_Lists[d][s], d);
        }
    }
}

static uint8_t *M_Write(
    const SCORE_FILE *const previous, const uint8_t key, const uint8_t random,
    size_t *const size)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
    const SCORE_RECORDS records = M_Records();
    ScoreFile_Write(&writer, previous, m_Lists, &records, key, random);
    uint8_t *const data = Writer_TakeBuffer(&writer, size);
    Result_Ignore(Writer_Close(&writer));
    return data;
}

TEST(read_decrypts_the_original_encryption)
{
    M_BYTES bytes;
    M_PutFileHeader(&bytes);
    M_PutHighScore(&bytes, 5000000, 1, 2, "ABCDEFGH");
    M_Seal(&bytes, 0x5a);
    SCORE_FILE file = M_Read(&bytes);
    SCORE_LIST list = {};
    ScoreFile_LoadHighScores(&file, &list, 1, 2);
    CHECK_EQ_INT(list.count, 1);
    CHECK_EQ_INT(list.entries[0].score, 5000000);
    CHECK_EQ_INT(list.entries[0].stage, 3);
    CHECK(memcmp(list.entries[0].name, "ABCDEFGH", 9) == 0);
    ScoreFile_Free(&file);
}

TEST(read_rejects_a_short_file)
{
    M_BYTES bytes = { .size = 19 };
    CHECK(M_ReadFails(&bytes));
}

TEST(read_rejects_a_wrong_checksum)
{
    M_BYTES bytes;
    M_PutFileHeader(&bytes);
    M_Seal(&bytes, 0x11);
    bytes.data[bytes.size - 1] ^= 1;
    CHECK(M_ReadFails(&bytes));
}

TEST(read_rejects_a_file_without_th6k)
{
    M_BYTES bytes;
    M_PutFileHeader(&bytes);
    memcpy(&bytes.data[20], "XXXX", 4);
    M_Seal(&bytes, 0x11);
    CHECK(M_ReadFails(&bytes));
}

TEST(read_rejects_a_size_past_the_data)
{
    M_BYTES bytes;
    M_PutFileHeader(&bytes);
    M_SealAs(&bytes, 0x11, bytes.size + 1);
    CHECK(M_ReadFails(&bytes));
}

// The search subtracts the size of the chunk it moves to. A TH6K chunk
// with a large size behind another chunk is therefore not found.
TEST(read_th6k_search_subtracts_the_next_chunk_size)
{
    M_BYTES bytes;
    M_PutFileHeader(&bytes);
    bytes.size = 20;
    M_PutChunkHeader(&bytes, "XXXX", 12, 0x10);
    M_PutChunkHeader(&bytes, "TH6K", 12, 0x10);
    M_Seal(&bytes, 0x22);
    SCORE_FILE file = M_Read(&bytes);
    ScoreFile_Free(&file);

    M_PutFileHeader(&bytes);
    bytes.size = 20;
    M_PutChunkHeader(&bytes, "XXXX", 12, 0x10);
    M_PutChunkHeader(&bytes, "TH6K", 30, 0x10);
    M_Seal(&bytes, 0x22);
    CHECK(M_ReadFails(&bytes));
}

TEST(read_stops_at_a_zero_size_chunk)
{
    M_BYTES bytes;
    M_PutFileHeader(&bytes);
    M_PutChunkHeader(&bytes, "XXXX", 0, 0x10);
    M_PutHighScore(&bytes, 5000000, 0, 0, "ABCDEFGH");
    M_Seal(&bytes, 0x33);
    SCORE_FILE file = M_Read(&bytes);
    SCORE_LIST list = {};
    ScoreFile_LoadHighScores(&file, &list, 0, 0);
    CHECK_EQ_INT(list.count, 0);
    ScoreFile_Free(&file);
}

TEST(load_skips_other_versions)
{
    M_BYTES bytes;
    M_PutFileHeader(&bytes);
    M_PutHighScore(&bytes, 5000000, 0, 0, "ABCDEFGH");
    bytes.data[bytes.size - 0x1c + 8] = 0x11;
    M_Seal(&bytes, 0x33);
    SCORE_FILE file = M_Read(&bytes);
    CHECK_EQ_INT(ScoreFile_GetHighScore(&file, 0, 0), 1000000);
    ScoreFile_Free(&file);
}

TEST(defaults_count_down_from_a_million)
{
    SCORE_LIST list;
    ScoreList_InitDefaults(&list, 3);
    CHECK_EQ_INT(list.count, 10);
    for (int32_t i = 0; i < 10; i++) {
        CHECK_EQ_INT(list.entries[i].score, 1000000 - i * 100000);
        CHECK_EQ_INT(list.entries[i].magic, HIGH_SCORE_DEFAULT_MAGIC);
        CHECK_EQ_INT(list.entries[i].difficulty, 3);
        CHECK_EQ_INT(list.entries[i].stage, 1);
        CHECK(memcmp(list.entries[i].name, "Nanashi ", 9) == 0);
    }
}

TEST(insert_places_a_tie_first)
{
    SCORE_LIST list;
    ScoreList_InitDefaults(&list, 0);
    const HIGH_SCORE entry = { .magic = HIGH_SCORE_MAGIC, .score = 800000 };
    CHECK_EQ_INT(ScoreList_Insert(&list, &entry), 2);
    CHECK_EQ_INT(list.entries[2].magic, HIGH_SCORE_MAGIC);
    CHECK_EQ_INT(list.entries[3].score, 800000);
    CHECK_EQ_INT(list.entries[3].magic, HIGH_SCORE_DEFAULT_MAGIC);
    CHECK_EQ_INT(list.entries[9].score, 200000);
}

TEST(insert_below_the_tenth_leaves_the_list)
{
    SCORE_LIST list;
    ScoreList_InitDefaults(&list, 0);
    const HIGH_SCORE entry = { .magic = HIGH_SCORE_MAGIC, .score = 100000 - 1 };
    CHECK_EQ_INT(ScoreList_Insert(&list, &entry), 10);
    CHECK_EQ_INT(list.entries[9].score, 100000);
    CHECK_EQ_INT(list.entries[9].magic, HIGH_SCORE_DEFAULT_MAGIC);
}

TEST(high_score_has_a_floor_of_a_million)
{
    const SCORE_FILE empty = {};
    CHECK_EQ_INT(ScoreFile_GetHighScore(&empty, 0, 0), 1000000);
    M_BYTES bytes;
    M_PutFileHeader(&bytes);
    M_PutHighScore(&bytes, 999999, 2, 0, "ABCDEFGH");
    M_PutHighScore(&bytes, 7000000, 2, 1, "ABCDEFGH");
    M_PutHighScore(&bytes, 9000000, 2, 2, "ABCDEFGH");
    M_Seal(&bytes, 0x33);
    SCORE_FILE file = M_Read(&bytes);
    CHECK_EQ_INT(ScoreFile_GetHighScore(&file, 2, 1), 7000000);
    CHECK_EQ_INT(ScoreFile_GetHighScore(&file, 2, 0), 1000000);
    CHECK_EQ_INT(ScoreFile_GetHighScore(&file, 3, 1), 1000000);
    ScoreFile_Free(&file);
}

TEST(spell_cards_missing_from_the_file_stay)
{
    for (int32_t i = 0; i < SPELL_CARD_COUNT; i++) {
        m_Cards[i] = (SPELL_CARD_RECORD) { .bonus = i, .attempts = 1 };
    }
    M_BYTES bytes;
    M_PutFileHeader(&bytes);
    M_PutSpellCard(&bytes, 5, "Sign");
    M_Seal(&bytes, 0x44);
    SCORE_FILE file = M_Read(&bytes);
    ScoreFile_LoadSpellCards(&file, m_Cards);
    CHECK_EQ_INT(m_Cards[4].bonus, 4);
    CHECK_EQ_INT(m_Cards[5].bonus, 300000);
    CHECK_EQ_INT(m_Cards[5].attempts, 7);
    CHECK_EQ_INT(m_Cards[5].captures, 3);
    CHECK_EQ_INT(m_Cards[5].checksum, 0x42);
    CHECK_EQ_STR(m_Cards[5].name, "Sign");
    CHECK_EQ_INT(m_Cards[5].name_bytes[5], 0xcc);
    ScoreFile_Free(&file);
}

TEST(spell_cards_stop_at_an_index_out_of_range)
{
    memset(m_Cards, 0, sizeof(m_Cards));
    M_BYTES bytes;
    M_PutFileHeader(&bytes);
    M_PutSpellCard(&bytes, 64, "Sign");
    M_PutSpellCard(&bytes, 2, "Sign");
    M_Seal(&bytes, 0x44);
    SCORE_FILE file = M_Read(&bytes);
    ScoreFile_LoadSpellCards(&file, m_Cards);
    CHECK_EQ_INT(m_Cards[2].attempts, 0);
    ScoreFile_Free(&file);
}

TEST(clears_default_to_stage_one_and_stop_at_a_bad_shot)
{
    M_BYTES bytes;
    M_PutFileHeader(&bytes);
    M_PutClear(&bytes, 1);
    M_PutClear(&bytes, 4);
    M_PutClear(&bytes, 2);
    M_Seal(&bytes, 0x55);
    SCORE_FILE file = M_Read(&bytes);
    ScoreFile_LoadClears(&file, m_Clears);
    CHECK_EQ_INT(m_Clears[0].without_continues[0], 1);
    CHECK_EQ_INT(m_Clears[0].with_continues[4], 1);
    CHECK_EQ_INT(m_Clears[0].shot, 0);
    CHECK_EQ_INT(m_Clears[1].without_continues[0], 10);
    CHECK_EQ_INT(m_Clears[1].with_continues[4], 19);
    CHECK_EQ_INT(m_Clears[2].without_continues[0], 1);
    ScoreFile_Free(&file);
}

TEST(practice_admits_difficulty_four_into_the_next_stage)
{
    M_BYTES bytes;
    M_PutFileHeader(&bytes);
    M_PutPractice(&bytes, 111, 1, 2, 3);
    M_PutPractice(&bytes, 222, 1, 0, 4);
    M_PutPractice(&bytes, 333, 3, 6, 4);
    M_PutPractice(&bytes, 444, 0, 0, 0);
    M_PutPractice(&bytes, 555, 0, 0, 5);
    M_PutPractice(&bytes, 666, 0, 1, 1);
    M_Seal(&bytes, 0x66);
    SCORE_FILE file = M_Read(&bytes);
    ScoreFile_LoadPractice(&file, m_Practice);
    CHECK_EQ_INT(m_Practice[1][2][3].score, 111);
    CHECK_EQ_INT(m_Practice[1][1][0].score, 222);
    CHECK_EQ_INT(m_Practice[1][1][0].difficulty, 4);
    CHECK_EQ_INT(m_Practice[0][0][0].score, 444);
    CHECK_EQ_INT(m_Practice[0][1][1].score, 0);
    CHECK_EQ_INT(m_Practice[2][3][1].stage, 3);
    CHECK_EQ_INT(m_Practice[2][3][1].difficulty, 1);
    ScoreFile_Free(&file);
}

TEST(write_sets_the_header_and_encrypts)
{
    M_ResetRecords();
    uint8_t previous_bytes[20] = { 0x77 };
    previous_bytes[7] = 0x88;
    const SCORE_FILE previous = { previous_bytes, 20 };
    size_t size;
    uint8_t *const data = M_Write(&previous, 0x9c, 0x3e, &size);
    CHECK_EQ_INT(size, 20 + 12 + 4 * 0x18);
    M_Open(data, (int32_t)size);
    CHECK_EQ_INT(data[0], 0x77);
    CHECK_EQ_INT(data[1], 0x9c);
    CHECK_EQ_INT(M_Get16(data, 4), 0x10);
    CHECK_EQ_INT(data[6], 0x3e);
    CHECK_EQ_INT(data[7], 0x88);
    CHECK_EQ_INT(M_Get32(data, 8), 20);
    CHECK_EQ_INT(M_Get32(data, 12), 0);
    CHECK_EQ_INT(M_Get32(data, 16), size);
    uint16_t checksum = 0;
    for (size_t i = 4; i < size; i++) {
        checksum = (uint16_t)(checksum + data[i]);
    }
    CHECK_EQ_INT(M_Get16(data, 2), checksum);
    CHECK(memcmp(&data[20], "TH6K", 4) == 0);
    CHECK_EQ_INT(M_Get16(data, 24), 12);
    CHECK_EQ_INT(M_Get16(data, 26), 12);
    CHECK_EQ_INT(data[28], 0x10);
    CHECK(memcmp(&data[32], "CLRD", 4) == 0);
    CHECK_EQ_INT(data[32 + 0x16], 0);
    CHECK_EQ_INT(data[32 + 3 * 0x18 + 0x16], 3);
    Allocator_Free(&g_HeapAllocator, data, size);
}

TEST(write_matches_the_original_encryption)
{
    M_ResetRecords();
    const SCORE_FILE previous = {};
    size_t size;
    uint8_t *const data = M_Write(&previous, 0xa5, 0x01, &size);
    M_BYTES expected = { .size = (int32_t)size };
    memcpy(expected.data, data, size);
    M_Open(expected.data, expected.size);
    expected.data[2] = 0;
    expected.data[3] = 0;
    M_Seal(&expected, 0xa5);
    CHECK(memcmp(expected.data, data, size) == 0);
    Allocator_Free(&g_HeapAllocator, data, size);
}

TEST(write_saves_scores_but_not_defaults)
{
    M_ResetRecords();
    HIGH_SCORE entry = {
        .magic = HIGH_SCORE_MAGIC,
        .is_new = true,
        .score = 850000,
        .shot = 9,
        .difficulty = 9,
        .stage = 99,
    };
    memcpy(entry.name, "NAME    ", 9);
    ScoreList_Insert(&m_Lists[3][2], &entry);
    const SCORE_FILE previous = {};
    size_t size;
    uint8_t *const data = M_Write(&previous, 0, 0, &size);
    CHECK_EQ_INT(size, 20 + 12 + 0x1c + 4 * 0x18);
    M_Open(data, (int32_t)size);
    const uint8_t *const chunk = &data[32];
    CHECK(memcmp(chunk, "HSCR", 4) == 0);
    CHECK_EQ_INT(M_Get16(chunk, 4), 0x1c);
    CHECK_EQ_INT(chunk[8], 0x10);
    CHECK_EQ_INT(chunk[9], 0);
    CHECK_EQ_INT(M_Get32(chunk, 12), 850000);
    CHECK_EQ_INT(chunk[16], 2);
    CHECK_EQ_INT(chunk[17], 3);
    CHECK_EQ_INT(chunk[18], 99);
    CHECK(memcmp(&chunk[19], "NAME    ", 9) == 0);
    Allocator_Free(&g_HeapAllocator, data, size);
}

TEST(write_saves_only_cards_with_their_magic)
{
    M_ResetRecords();
    m_Cards[7].header.magic = 0x4b544143;
    m_Cards[7].index = 99;
    m_Cards[7].attempts = 5;
    memcpy(m_Cards[7].name_bytes, "Card\0\x12\x34", 7);
    const SCORE_FILE previous = {};
    size_t size;
    uint8_t *const data = M_Write(&previous, 0, 0, &size);
    CHECK_EQ_INT(size, 20 + 12 + 4 * 0x18 + 0x40);
    M_Open(data, (int32_t)size);
    const uint8_t *const chunk = &data[32 + 4 * 0x18];
    CHECK(memcmp(chunk, "CATK", 4) == 0);
    CHECK_EQ_INT(M_Get16(chunk, 4), 0x40);
    CHECK_EQ_INT(M_Get16(chunk, 16), 7);
    CHECK_EQ_INT(chunk[24 + 5], 0x12);
    CHECK_EQ_INT(M_Get16(chunk, 60), 5);
    Allocator_Free(&g_HeapAllocator, data, size);
}

TEST(write_then_read_round_trips_every_record)
{
    M_ResetRecords();
    HIGH_SCORE entry = { .magic = HIGH_SCORE_MAGIC, .score = 12345678 };
    memcpy(entry.name, "ROUND   ", 9);
    ScoreList_Insert(&m_Lists[4][1], &entry);
    m_Clears[2].without_continues[3] = 99;
    m_Clears[2].with_continues[1] = 6;
    m_Practice[3][5][2].score = 4242;
    m_Cards[63].header.magic = 0x4b544143;
    m_Cards[63].captures = 9;
    m_Cards[63].history[0] = 3;
    memcpy(m_Cards[63].name_bytes, "Last", 5);
    const SCORE_FILE previous = {};
    size_t size;
    uint8_t *const data = M_Write(&previous, 0x12, 0x34, &size);
    M_BYTES bytes = { .size = (int32_t)size };
    memcpy(bytes.data, data, size);
    Allocator_Free(&g_HeapAllocator, data, size);

    SCORE_FILE file = M_Read(&bytes);
    SCORE_LIST list;
    ScoreList_InitDefaults(&list, 4);
    ScoreFile_LoadHighScores(&file, &list, 1, 4);
    CHECK_EQ_INT(list.entries[0].score, 12345678);
    CHECK(memcmp(list.entries[0].name, "ROUND   ", 9) == 0);
    CHECK_EQ_INT(list.entries[1].magic, HIGH_SCORE_DEFAULT_MAGIC);
    CLEAR_RECORD clears[SCORE_SHOT_COUNT];
    ScoreFile_LoadClears(&file, clears);
    CHECK(memcmp(clears, m_Clears, sizeof(clears)) == 0);
    PRACTICE_RECORD practice[SCORE_SHOT_COUNT][SCORE_STAGE_COUNT]
                            [SCORE_PRACTICE_DIFFICULTY_COUNT];
    ScoreFile_LoadPractice(&file, practice);
    CHECK_EQ_INT(practice[3][5][2].score, 4242);
    SPELL_CARD_RECORD cards[SPELL_CARD_COUNT] = {};
    ScoreFile_LoadSpellCards(&file, cards);
    CHECK_EQ_INT(cards[63].index, 63);
    CHECK_EQ_INT(cards[63].captures, 9);
    CHECK_EQ_INT(cards[63].history[0], 3);
    CHECK_EQ_STR(cards[63].name, "Last");
    CHECK_EQ_INT(cards[62].header.magic, 0);
    ScoreFile_Free(&file);
}
