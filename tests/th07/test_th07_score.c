#include "th07/screens/result/score.h"

#include "test.h"

#include "replay_lzss_literals.h"
#include "th07/exe_identity.h"

#include "core/base/memory.h"

#include <string.h>

#define M_HEADER_SIZE 0x1c

typedef struct {
    uint8_t data[0x4000];
    int32_t size;
} M_BYTES;

static SPELL_CARD_RECORD m_Cards[SPELL_CARD_COUNT];
static CLEAR_RECORD m_Clears[SCORE_SHOT_COUNT];
static PRACTICE_RECORD m_Practice[SCORE_SHOT_COUNT][SCORE_STAGE_COUNT]
                                 [SCORE_PRACTICE_DIFFICULTY_COUNT];
static SCORE_LIST m_Lists[SCORE_DIFFICULTY_COUNT][SCORE_SHOT_COUNT];
static LAST_NAME_RECORD m_LastName;
static PLAY_STATS_RECORD m_PlayStats;

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

static void M_PutN(M_BYTES *const bytes, const int32_t count, const uint8_t v)
{
    for (int32_t i = 0; i < count; i++) {
        M_Put8(bytes, v);
    }
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
    M_PutN(bytes, 3, 0);
}

static void M_Start(M_BYTES *const bytes)
{
    memset(bytes, 0, sizeof(*bytes));
    M_PutChunkHeader(bytes, "TH7K", 12, 1);
}

static void M_PutHighScore(
    M_BYTES *const bytes, const uint32_t score, const int32_t shot,
    const int32_t difficulty, const uint32_t version)
{
    M_PutChunkHeader(bytes, "HSCR", 0x28, version);
    M_Put32(bytes, score);
    M_Put32(bytes, 0x3f800000); // 1.0f
    M_Put8(bytes, (uint32_t)shot);
    M_Put8(bytes, (uint32_t)difficulty);
    M_Put8(bytes, 3);
    for (int32_t i = 0; i < 9; i++) {
        M_Put8(bytes, (uint8_t)"ABCDEFGH"[i]);
    }
    for (int32_t i = 0; i < 6; i++) {
        M_Put8(bytes, (uint8_t)"11/16"[i]);
    }
    M_Put8(bytes, 2);
    M_Put8(bytes, 0);
}

static void M_PutSpellCard(M_BYTES *const bytes, const int32_t index)
{
    M_PutChunkHeader(bytes, "CATK", 0x78, 1);
    for (int32_t i = 0; i < 7; i++) {
        M_Put32(bytes, (uint32_t)(1000 * i));
    }
    M_Put16(bytes, (uint32_t)index);
    M_Put8(bytes, 0x42);
    M_Put8(bytes, 'C');
    M_Put8(bytes, 0);
    M_PutN(bytes, 0x31 - 2, 0xcc);
    for (int32_t i = 0; i < 14; i++) {
        M_Put16(bytes, (uint32_t)i);
    }
}

static void M_PutClear(M_BYTES *const bytes, const int32_t shot)
{
    M_PutChunkHeader(bytes, "CLRD", 0x1c, 1);
    for (int32_t i = 0; i < 12; i++) {
        M_Put8(bytes, (uint32_t)(10 + i));
    }
    M_Put8(bytes, (uint32_t)shot);
    M_PutN(bytes, 3, 0);
}

static void M_PutPractice(
    M_BYTES *const bytes, const uint32_t score, const int32_t shot,
    const int32_t stage, const int32_t difficulty)
{
    M_PutChunkHeader(bytes, "PSCR", 0x18, 1);
    M_Put32(bytes, 5);
    M_Put32(bytes, score);
    M_Put8(bytes, (uint32_t)shot);
    M_Put8(bytes, (uint32_t)difficulty);
    M_Put8(bytes, (uint32_t)stage);
    M_Put8(bytes, 0);
}

static void M_PutPlayStats(M_BYTES *const bytes, const uint32_t hours)
{
    M_PutChunkHeader(bytes, "PLST", 0x160, 1);
    M_PutN(bytes, 16, 0);
    M_Put32(bytes, hours);
    M_PutN(bytes, 0x160 - 12 - 20, 0);
}

// Encrypts size bytes of data as SaveScoreDat does.
static void M_Encrypt(uint8_t *const data, const int32_t size)
{
    uint8_t *p = data + 1;
    uint8_t rolling = *p;
    for (int32_t m = size - 2; m > 0; m--, p++) {
        const uint8_t previous = p[1];
        rolling = (uint8_t)(((rolling & 0xe0) >> 5) | ((rolling & 0x1f) << 3));
        p[1] ^= rolling;
        rolling = (uint8_t)(rolling + previous);
    }
}

// Packs chunks as LZSS literals behind a header, as SaveScoreDat lays the
// file out, and encrypts it with key as SaveScoreDat does.
static void M_Seal(
    const M_BYTES *const chunks, const uint8_t key, M_BYTES *const out)
{
    memset(out, 0, sizeof(*out));
    uint8_t *const data = out->data;
    const size_t packed = ReplayTestLzss_EncodeLiterals(
        chunks->data, (size_t)chunks->size, data + M_HEADER_SIZE);
    out->size = M_HEADER_SIZE + (int32_t)packed;
    data[1] = key;
    data[4] = 0xb;
    data[8] = M_HEADER_SIZE;
    const uint32_t fields[] = {
        (uint32_t)(M_HEADER_SIZE + chunks->size),
        (uint32_t)chunks->size,
        (uint32_t)packed,
    };
    for (int32_t i = 0; i < 3; i++) {
        for (int32_t b = 0; b < 4; b++) {
            data[0x10 + 4 * i + b] = (uint8_t)(fields[i] >> (8 * b));
        }
    }
    uint16_t checksum = 0;
    for (int32_t i = 4; i < out->size; i++) {
        checksum = (uint16_t)(checksum + data[i]);
    }
    data[2] = (uint8_t)checksum;
    data[3] = (uint8_t)(checksum >> 8);
    M_Encrypt(data, out->size);
}

static SCORE_FILE M_ReadBytes(const uint8_t *const data, const size_t size)
{
    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    SCORE_FILE file;
    CHECK(Result_Log(ScoreFile_Read(&reader, &file), LOG_LEVEL_ERROR));
    return file;
}

static SCORE_FILE M_Read(const M_BYTES *const chunks)
{
    M_BYTES sealed;
    M_Seal(chunks, 0x5a, &sealed);
    return M_ReadBytes(sealed.data, (size_t)sealed.size);
}

static bool M_ReadFails(const M_BYTES *const bytes)
{
    READER reader;
    Reader_OpenBuffer(&reader, bytes->data, (size_t)bytes->size, nullptr);
    SCORE_FILE file;
    const RESULT result = ScoreFile_Read(&reader, &file);
    const bool failed = !IS_OK(result);
    Result_Ignore(result);
    CHECK(file.data == nullptr);
    ScoreFile_Free(&file);
    return failed;
}

static bool M_ChunksFail(const M_BYTES *const chunks)
{
    M_BYTES sealed;
    M_Seal(chunks, 0x5a, &sealed);
    return M_ReadFails(&sealed);
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
    memset(&m_LastName, 0, sizeof(m_LastName));
    m_LastName.header = (SCORE_RECORD_HEADER) {
        .magic = 0x4d4e534c, .size = 0x18, .size2 = 0x18, .version = 1
    };
    memcpy(m_LastName.name, "        ", 9);
    memset(&m_PlayStats, 0, sizeof(m_PlayStats));
    m_PlayStats.header = (SCORE_RECORD_HEADER) {
        .magic = 0x54534c50, .size = 0x160, .size2 = 0x160, .version = 1
    };
}

static uint8_t *M_Write(
    const SCORE_FILE *const previous, const uint8_t key, const uint8_t random,
    size_t *const size)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    const SCORE_RECORDS records = {
        .spell_cards = m_Cards,
        .clears = m_Clears,
        .practice = m_Practice,
        .last_name = &m_LastName,
        .play_stats = &m_PlayStats,
    };
    ScoreFile_Write(&writer, previous, m_Lists, &records, key, random);
    uint8_t *const data = Writer_TakeBuffer(&writer, size);
    Result_Ignore(Writer_Close(&writer));
    return data;
}

// Writes the records and reads the file back.
static SCORE_FILE M_WriteAndRead(void)
{
    const SCORE_FILE previous = {};
    size_t size;
    uint8_t *const data = M_Write(&previous, 0x12, 0x34, &size);
    const SCORE_FILE file = M_ReadBytes(data, size);
    Allocator_Free(&g_HeapAllocator, data, size);
    return file;
}

TEST(read_unpacks_a_hand_built_file)
{
    M_BYTES chunks;
    M_Start(&chunks);
    M_PutHighScore(&chunks, 5000000, 1, 2, 1);
    SCORE_FILE file = M_Read(&chunks);
    CHECK_EQ_INT(file.size, M_HEADER_SIZE + chunks.size);
    CHECK(memcmp(file.data + M_HEADER_SIZE, chunks.data, chunks.size) == 0);
    SCORE_LIST list = {};
    ScoreFile_LoadHighScores(&file, &list, 1, 2);
    CHECK_EQ_INT(list.count, 1);
    const HIGH_SCORE *const entry = &list.entries[0];
    CHECK_EQ_INT(entry->score, 5000000);
    CHECK(entry->slowdown == 1.0f);
    CHECK_EQ_INT(entry->stage, 3);
    CHECK_EQ_STR((const char *)entry->name, "ABCDEFGH");
    CHECK_EQ_STR(entry->date, "11/16");
    CHECK_EQ_INT(entry->continues_used, 2);
    ScoreFile_Free(&file);
}

TEST(read_rejects_a_short_file)
{
    M_BYTES bytes = { .size = M_HEADER_SIZE - 1 };
    CHECK(M_ReadFails(&bytes));
}

TEST(read_rejects_a_wrong_checksum)
{
    M_BYTES chunks;
    M_Start(&chunks);
    M_BYTES sealed;
    M_Seal(&chunks, 0x5a, &sealed);
    sealed.data[sealed.size - 1] ^= 1;
    CHECK(M_ReadFails(&sealed));
}

TEST(read_rejects_a_file_without_a_version_1_th7k)
{
    M_BYTES chunks = {};
    M_PutClear(&chunks, 0);
    CHECK(M_ChunksFail(&chunks));
    M_Start(&chunks);
    M_PutChunkHeader(&chunks, "TH7K", 12, 2);
    CHECK(M_ChunksFail(&chunks));
}

TEST(read_rejects_a_zero_size_chunk)
{
    M_BYTES chunks;
    M_Start(&chunks);
    M_PutChunkHeader(&chunks, "XXXX", 0, 1);
    CHECK(M_ChunksFail(&chunks));
}

TEST(read_rejects_a_chunk_header_past_the_data)
{
    M_BYTES chunks;
    M_Start(&chunks);
    M_PutChunkHeader(&chunks, "XXXX", 6, 1);
    CHECK(M_ChunksFail(&chunks));
}

TEST(read_keeps_the_game_version_without_a_check)
{
    M_BYTES chunks;
    M_Start(&chunks);
    M_PutChunkHeader(&chunks, "VRSM", 0x1c, 1);
    M_PutN(&chunks, 0x10, 0xee);
    SCORE_FILE file = M_Read(&chunks);
    ScoreFile_Free(&file);
}

TEST(load_skips_other_versions)
{
    M_BYTES chunks;
    M_Start(&chunks);
    M_PutHighScore(&chunks, 7000000, 0, 0, 2);
    SCORE_FILE file = M_Read(&chunks);
    uint8_t continues = 9;
    CHECK_EQ_INT(ScoreFile_GetHighScore(&file, 0, 0, &continues), 100000);
    CHECK_EQ_INT(continues, 0);
    ScoreFile_Free(&file);
}

TEST(defaults_count_down_from_100000)
{
    SCORE_LIST list;
    ScoreList_InitDefaults(&list, 3);
    CHECK_EQ_INT(list.count, 10);
    CHECK_EQ_INT(list.entries[0].score, 100000);
    CHECK_EQ_INT(list.entries[9].score, 10000);
    CHECK_EQ_INT(list.entries[9].difficulty, 3);
    CHECK_EQ_INT(list.entries[9].magic, HIGH_SCORE_DEFAULT_MAGIC);
    CHECK_EQ_STR((const char *)list.entries[0].name, "--------");
    CHECK_EQ_STR(list.entries[0].date, "--/--");
}

TEST(insert_places_a_tie_first)
{
    SCORE_LIST list;
    ScoreList_InitDefaults(&list, 0);
    const HIGH_SCORE entry = { .magic = HIGH_SCORE_MAGIC, .score = 50000 };
    CHECK_EQ_INT(ScoreList_Insert(&list, &entry), 5);
    CHECK_EQ_INT(list.entries[5].magic, HIGH_SCORE_MAGIC);
    CHECK_EQ_INT(list.entries[6].score, 50000);
    CHECK_EQ_INT(list.entries[9].score, 20000);
}

TEST(high_score_has_a_floor_of_100000_and_takes_the_best_continues)
{
    M_BYTES chunks;
    M_Start(&chunks);
    M_PutHighScore(&chunks, 90000, 4, 1, 1);
    SCORE_FILE file = M_Read(&chunks);
    uint8_t continues;
    CHECK_EQ_INT(ScoreFile_GetHighScore(&file, 4, 1, &continues), 100000);
    CHECK_EQ_INT(continues, 2);
    ScoreFile_Free(&file);
}

TEST(spell_cards_stop_at_an_index_out_of_range)
{
    M_BYTES chunks;
    M_Start(&chunks);
    M_PutSpellCard(&chunks, 0x8c);
    M_PutSpellCard(&chunks, 0x8d);
    M_PutSpellCard(&chunks, 3);
    SCORE_FILE file = M_Read(&chunks);
    memset(m_Cards, 0, sizeof(m_Cards));
    m_Cards[3].attempts[0] = 77;
    ScoreFile_LoadSpellCards(&file, m_Cards);
    const SPELL_CARD_RECORD *const card = &m_Cards[0x8c];
    CHECK_EQ_INT(card->header.magic, SPELL_CARD_MAGIC);
    CHECK_EQ_INT(card->max_bonus[6], 6000);
    CHECK_EQ_INT(card->checksum, 0x42);
    CHECK_EQ_INT(card->name[0], 'C');
    CHECK_EQ_INT(card->name[2], 0xcc);
    CHECK_EQ_INT(card->attempts[6], 6);
    CHECK_EQ_INT(card->captures[6], 13);
    CHECK_EQ_INT(m_Cards[3].attempts[0], 77);
    ScoreFile_Free(&file);
}

TEST(clears_default_to_stage_one_and_stop_at_a_bad_shot)
{
    M_BYTES chunks;
    M_Start(&chunks);
    M_PutClear(&chunks, 5);
    M_PutClear(&chunks, 6);
    M_PutClear(&chunks, 0);
    SCORE_FILE file = M_Read(&chunks);
    ScoreFile_LoadClears(&file, m_Clears);
    CHECK_EQ_INT(m_Clears[5].no_continues[0], 10);
    CHECK_EQ_INT(m_Clears[5].with_continues[5], 21);
    CHECK_EQ_INT(m_Clears[0].no_continues[0], 1);
    CHECK_EQ_INT(m_Clears[0].no_continues[5], 0);
    CHECK_EQ_INT(m_Clears[0].header.magic, 0x44524c43);
    CHECK_EQ_INT(m_Clears[0].header.size, 0x1c);
    ScoreFile_Free(&file);
}

TEST(practice_admits_difficulty_four_into_the_next_stage)
{
    M_BYTES chunks;
    M_Start(&chunks);
    M_PutPractice(&chunks, 111, 1, 2, 4);
    M_PutPractice(&chunks, 222, 5, 6, 0);
    M_PutPractice(&chunks, 333, 0, 0, 5);
    M_PutPractice(&chunks, 444, 0, 0, 0);
    SCORE_FILE file = M_Read(&chunks);
    ScoreFile_LoadPractice(&file, m_Practice);
    CHECK_EQ_INT(m_Practice[1][3][0].score, 111);
    CHECK_EQ_INT(m_Practice[1][3][0].stage, 2);
    CHECK_EQ_INT(m_Practice[1][3][0].play_count, 5);
    CHECK_EQ_INT(m_Practice[0][0][0].score, 0);
    CHECK_EQ_INT(m_Practice[2][1][3].stage, 1);
    ScoreFile_Free(&file);
}

TEST(play_stats_take_the_last_record_and_last_name_the_first)
{
    M_BYTES chunks;
    M_Start(&chunks);
    M_PutPlayStats(&chunks, 3);
    M_PutChunkHeader(&chunks, "LSNM", 0x18, 1);
    M_PutN(&chunks, 12, 'A');
    M_PutChunkHeader(&chunks, "LSNM", 0x18, 1);
    M_PutN(&chunks, 12, 'B');
    M_PutPlayStats(&chunks, 8);
    SCORE_FILE file = M_Read(&chunks);
    PLAY_STATS_RECORD stats = {};
    ScoreFile_LoadPlayStats(&file, &stats);
    CHECK_EQ_INT(stats.play_hours, 8);
    LAST_NAME_RECORD name = {};
    CHECK(ScoreFile_LoadLastName(&file, &name));
    CHECK_EQ_INT(name.name[11], 'A');
    ScoreFile_Free(&file);
    const SCORE_FILE empty = {};
    stats.play_hours = 9;
    ScoreFile_LoadPlayStats(&empty, &stats);
    CHECK_EQ_INT(stats.play_hours, 9);
    CHECK(!ScoreFile_LoadLastName(&empty, &name));
}

TEST(load_stops_at_a_record_past_the_data)
{
    M_BYTES chunks;
    M_Start(&chunks);
    M_PutClear(&chunks, 1);
    M_PutChunkHeader(&chunks, "CLRD", 12, 1);
    SCORE_FILE file = M_Read(&chunks);
    ScoreFile_LoadClears(&file, m_Clears);
    CHECK_EQ_INT(m_Clears[1].no_continues[0], 10);
    ScoreFile_Free(&file);
}

TEST(write_sets_the_header_and_matches_the_original_encryption)
{
    M_ResetRecords();
    uint8_t previous_bytes[M_HEADER_SIZE] = { 0x77 };
    previous_bytes[7] = 0x88;
    previous_bytes[0xc] = 0x99;
    previous_bytes[0xf] = 0xaa;
    const SCORE_FILE previous = { previous_bytes, M_HEADER_SIZE };
    size_t size;
    uint8_t *const data = M_Write(&previous, 0x9c, 0x3e, &size);
    SCORE_FILE file = M_ReadBytes(data, size);
    const uint8_t *const header = file.data;
    CHECK_EQ_INT(header[0], 0x77);
    CHECK_EQ_INT(header[1], 0x9c);
    CHECK_EQ_INT(M_Get16(header, 4), 0xb);
    CHECK_EQ_INT(header[6], 0x3e);
    CHECK_EQ_INT(header[7], 0x88);
    CHECK_EQ_INT(M_Get32(header, 8), M_HEADER_SIZE);
    CHECK_EQ_INT(M_Get32(header, 0xc), 0xaa000099);
    CHECK_EQ_INT(M_Get32(header, 0x10), file.size);
    CHECK_EQ_INT(M_Get32(header, 0x14), file.size - M_HEADER_SIZE);
    CHECK_EQ_INT(M_Get32(header, 0x18), size - M_HEADER_SIZE);

    M_BYTES expected = { .size = (int32_t)size };
    memcpy(expected.data, data, size);
    uint8_t key = 0;
    for (int32_t i = 2; i < expected.size; i++) {
        key = (uint8_t)(key + expected.data[i - 1]);
        key = (uint8_t)(((key & 0xe0) >> 5) | ((key & 0x1f) << 3));
        expected.data[i] ^= key;
    }
    uint16_t checksum = 0;
    for (int32_t i = 4; i < expected.size; i++) {
        checksum = (uint16_t)(checksum + expected.data[i]);
    }
    CHECK_EQ_INT(M_Get16(expected.data, 2), checksum);
    M_Encrypt(expected.data, expected.size);
    CHECK(memcmp(expected.data, data, size) == 0);
    ScoreFile_Free(&file);
    Allocator_Free(&g_HeapAllocator, data, size);
}

TEST(write_lays_out_every_chunk_in_order)
{
    M_ResetRecords();
    m_Cards[7].header.magic = SPELL_CARD_MAGIC;
    m_Cards[7].index = 99;
    m_Practice[2][3][1].score = 4242;
    SCORE_FILE file = M_WriteAndRead();
    const uint8_t *chunk = file.data + M_HEADER_SIZE;
    const char *const order[] = {
        "TH7K", "CLRD", "CLRD", "CLRD", "CLRD", "CLRD",
        "CLRD", "CATK", "PSCR", "LSNM", "PLST", "VRSM",
    };
    const uint32_t sizes[] = {
        12, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x1c, 0x78, 0x18, 0x18, 0x160, 0x1c,
    };
    for (int32_t i = 0; i < 12; i++) {
        CHECK(memcmp(chunk, order[i], 4) == 0);
        CHECK_EQ_INT(M_Get16(chunk, 4), sizes[i]);
        CHECK_EQ_INT(M_Get16(chunk, 6), sizes[i]);
        CHECK_EQ_INT(chunk[8], 1);
        chunk += sizes[i];
    }
    CHECK(chunk == file.data + file.size);
    const uint8_t *const card = chunk - 0x1c - 0x160 - 0x18 - 0x18 - 0x78;
    CHECK_EQ_INT(M_Get16(card, 0x28), 7);
    const uint8_t *const version = chunk - 0x1c;
    CHECK(memcmp(version + 12, "0100b", 6) == 0);
    CHECK_EQ_INT(M_Get32(version, 20), TH07_EXE_SIZE);
    CHECK_EQ_INT(M_Get32(version, 24), TH07_EXE_CHECKSUM);
    ScoreFile_Free(&file);
}

TEST(write_saves_the_first_ten_scores_but_not_defaults)
{
    M_ResetRecords();
    HIGH_SCORE entry = {
        .magic = HIGH_SCORE_MAGIC,
        .is_new = true,
        .score = 85000,
        .shot = 9,
        .difficulty = 9,
        .stage = 99,
        .continues_used = 3,
    };
    ScoreList_Insert(&m_Lists[3][2], &entry);
    entry.score = 5000;
    ScoreList_Insert(&m_Lists[3][2], &entry);
    SCORE_FILE file = M_WriteAndRead();
    const uint8_t *const chunk = file.data + M_HEADER_SIZE + 12;
    CHECK(memcmp(chunk, "HSCR", 4) == 0);
    CHECK_EQ_INT(chunk[9], 0);
    CHECK(memcmp(chunk + 0x28, "CLRD", 4) == 0);
    SCORE_LIST list = {};
    ScoreFile_LoadHighScores(&file, &list, 2, 3);
    CHECK_EQ_INT(list.count, 1);
    CHECK_EQ_INT(list.entries[0].score, 85000);
    CHECK_EQ_INT(list.entries[0].stage, 99);
    CHECK_EQ_INT(list.entries[0].continues_used, 3);
    CHECK(!list.entries[0].is_new);
    ScoreFile_Free(&file);
}

TEST(write_then_read_round_trips_every_record)
{
    M_ResetRecords();
    HIGH_SCORE entry = {
        .magic = HIGH_SCORE_MAGIC,
        .score = 12345678,
        .slowdown = 2.5f,
        .stage = 4,
        .reserved = { 1, 2 },
        .continues_used = 1,
        .reserved_end = 7,
    };
    memcpy(entry.name, "ROUND   ", 9);
    memcpy(entry.date, "02/29", 6);
    ScoreList_Insert(&m_Lists[5][1], &entry);
    m_Clears[3].no_continues[4] = 99;
    m_Clears[3].reserved[1] = 0x55;
    m_Practice[3][5][2].score = 4242;
    m_Practice[3][5][2].play_count = 17;
    SPELL_CARD_RECORD *const card = &m_Cards[0x8c];
    card->header.magic = SPELL_CARD_MAGIC;
    card->header.reserved[2] = 0x66;
    card->max_bonus[2] = 123456;
    card->checksum = 0x99;
    memcpy(card->name, "Last\0\x12", 6);
    card->attempts[6] = 12;
    card->captures[2] = 9;
    m_LastName.name[0] = 'Z';
    m_PlayStats.run_milliseconds = 999;
    m_PlayStats.counts[6].practices = 31;
    m_PlayStats.counts[2].plays[3] = 8;
    SCORE_FILE file = M_WriteAndRead();

    SCORE_LIST list;
    ScoreList_InitDefaults(&list, 5);
    ScoreFile_LoadHighScores(&file, &list, 1, 5);
    HIGH_SCORE expected = entry;
    expected.difficulty = 5;
    expected.shot = 1;
    CHECK(memcmp(&list.entries[0], &expected, sizeof(expected)) == 0);
    CHECK_EQ_INT(list.entries[1].magic, HIGH_SCORE_DEFAULT_MAGIC);
    CLEAR_RECORD clears[SCORE_SHOT_COUNT];
    ScoreFile_LoadClears(&file, clears);
    CHECK(memcmp(clears, m_Clears, sizeof(clears)) == 0);
    static PRACTICE_RECORD practice[SCORE_SHOT_COUNT][SCORE_STAGE_COUNT]
                                   [SCORE_PRACTICE_DIFFICULTY_COUNT];
    ScoreFile_LoadPractice(&file, practice);
    CHECK(memcmp(practice, m_Practice, sizeof(practice)) == 0);
    static SPELL_CARD_RECORD cards[SPELL_CARD_COUNT];
    memset(cards, 0, sizeof(cards));
    ScoreFile_LoadSpellCards(&file, cards);
    SPELL_CARD_RECORD expected_card = *card;
    expected_card.header.size = 0x78;
    expected_card.header.size2 = 0x78;
    expected_card.header.version = 1;
    expected_card.index = 0x8c;
    CHECK(memcmp(&cards[0x8c], &expected_card, sizeof(expected_card)) == 0);
    CHECK_EQ_INT(cards[0x8b].header.magic, 0);
    LAST_NAME_RECORD name;
    CHECK(ScoreFile_LoadLastName(&file, &name));
    CHECK(memcmp(&name, &m_LastName, sizeof(name)) == 0);
    PLAY_STATS_RECORD stats;
    ScoreFile_LoadPlayStats(&file, &stats);
    CHECK(memcmp(&stats, &m_PlayStats, sizeof(stats)) == 0);
    ScoreFile_Free(&file);
}
