#include "th06/screens/result/score.h"

#include "core/base/cp932.h"
#include "core/base/memory.h"

#include <string.h>

#define M_HEADER_SIZE 0x14
#define M_VERSION 0x10
#define M_TH6K_MAGIC 0x4b364854
#define M_CLEAR_MAGIC 0x44524c43
#define M_SPELL_CARD_MAGIC 0x4b544143
#define M_PRACTICE_MAGIC 0x52435350
#define M_CHUNK_HEADER_SIZE 12
#define M_TH6K_SIZE 12
#define M_HIGH_SCORE_SIZE 0x1c
#define M_CLEAR_SIZE 0x18
#define M_SPELL_CARD_SIZE 0x40
#define M_PRACTICE_SIZE 0x14
#define M_PRACTICE_COUNT                                                       \
    (SCORE_SHOT_COUNT * SCORE_STAGE_COUNT * SCORE_PRACTICE_DIFFICULTY_COUNT)

// Walks the chunks of a score file as the original's loaders do.
typedef struct {
    const SCORE_FILE *file;
    int32_t position;
    int32_t left;
    uint32_t magic;
    uint16_t size;
    uint8_t version;
} M_CHUNKS;

static uint8_t M_RotateKey(const uint8_t key)
{
    return (uint8_t)(((key & 0xe0) >> 5) | ((key & 0x1f) << 3));
}

static READER M_ReaderAt(const SCORE_FILE *const file, const int32_t position)
{
    READER reader;
    Reader_OpenBuffer(&reader, file->data, (size_t)file->size, nullptr);
    Reader_Seek(&reader, (size_t)position);
    return reader;
}

static int32_t M_I32At(const SCORE_FILE *const file, const int32_t position)
{
    READER reader = M_ReaderAt(file, position);
    return Reader_ReadI32(&reader);
}

static M_CHUNKS M_FirstChunk(const SCORE_FILE *const file)
{
    M_CHUNKS chunks = { .file = file };
    if (file->data != nullptr) {
        chunks.position = M_I32At(file, 8);
        chunks.left = M_I32At(file, 16) - chunks.position;
    }
    return chunks;
}

// Reads the header of the chunk at the position into chunks. Returns
// false once the scan ends.
static bool M_ReadChunk(M_CHUNKS *const chunks)
{
    if (chunks->left <= 0 || chunks->position < 0
        || chunks->position > chunks->file->size - M_CHUNK_HEADER_SIZE) {
        return false;
    }
    READER reader = M_ReaderAt(chunks->file, chunks->position);
    chunks->magic = Reader_ReadU32(&reader);
    chunks->size = Reader_ReadU16(&reader);
    Reader_Skip(&reader, 2);
    chunks->version = Reader_ReadU8(&reader);
    return chunks->size != 0;
}

static void M_NextChunk(M_CHUNKS *const chunks)
{
    chunks->left -= chunks->size;
    chunks->position += chunks->size;
}

// Prepares a reader over the record of size bytes at the chunk. Returns
// false when the record reaches past the data.
static bool M_Record(
    const M_CHUNKS *const chunks, const int32_t size, READER *const out)
{
    if (chunks->position > chunks->file->size - size) {
        return false;
    }
    *out = M_ReaderAt(chunks->file, chunks->position);
    return true;
}

static void M_ReadHeader(READER *const reader, SCORE_RECORD_HEADER *const out)
{
    out->magic = Reader_ReadU32(reader);
    out->size = Reader_ReadU16(reader);
    out->size2 = Reader_ReadU16(reader);
    out->version = Reader_ReadU8(reader);
    Reader_ReadBytes(reader, out->reserved, sizeof(out->reserved));
}

static void M_ReadHighScore(READER *const reader, HIGH_SCORE *const out)
{
    out->magic = Reader_ReadU32(reader);
    Reader_Skip(reader, 5);
    out->is_new = Reader_ReadU8(reader) != 0;
    Reader_ReadBytes(reader, out->reserved, sizeof(out->reserved));
    out->score = Reader_ReadU32(reader);
    out->shot = Reader_ReadU8(reader);
    out->difficulty = Reader_ReadU8(reader);
    out->stage = Reader_ReadU8(reader);
    Reader_ReadBytes(reader, out->name, sizeof(out->name));
}

static uint16_t M_U16At(const SCORE_FILE *const file, const int32_t position)
{
    READER reader = M_ReaderAt(file, position);
    return Reader_ReadU16(&reader);
}

// Returns whether the original's search finds a TH6K chunk. After each
// chunk it subtracts the size of the chunk it moves to, not the size of the
// chunk it leaves.
static bool M_HasTh6k(const SCORE_FILE *const file)
{
    int32_t position = M_I32At(file, 8);
    int32_t left = M_I32At(file, 16) - position;
    while (left > 0) {
        if (position < 0 || position > file->size - M_CHUNK_HEADER_SIZE) {
            return false;
        }
        READER reader = M_ReaderAt(file, position);
        if (Reader_ReadU32(&reader) == M_TH6K_MAGIC) {
            break;
        }
        const uint16_t size = Reader_ReadU16(&reader);
        if (size == 0) {
            return false;
        }
        position += size;
        if (position > file->size - M_CHUNK_HEADER_SIZE) {
            return false;
        }
        left -= M_U16At(file, position + 4);
    }
    return left > 0;
}

static RESULT M_Decrypt(SCORE_FILE *const file)
{
    uint8_t *const data = file->data;
    uint8_t key = 0;
    uint16_t checksum = 0;
    for (int32_t i = 2; i < file->size; i++) {
        key = M_RotateKey((uint8_t)(key + data[i - 1]));
        data[i] ^= key;
        if (i >= 4) {
            checksum = (uint16_t)(checksum + data[i]);
        }
    }
    READER reader = M_ReaderAt(file, 2);
    FAIL_IF(
        Reader_ReadU16(&reader) != checksum,
        "score file checksum does not match");
    const int32_t data_offset = M_I32At(file, 8);
    const int32_t file_size = M_I32At(file, 16);
    FAIL_IF(
        data_offset < 0 || file_size < 0 || file_size > file->size,
        "score file offsets reach past its data");
    FAIL_IF(!M_HasTh6k(file), "score file has no TH6K chunk");
    return OK;
}

RESULT ScoreFile_Read(READER *const reader, SCORE_FILE *const out)
{
    *out = (SCORE_FILE) {};
    const size_t size = Reader_Remaining(reader);
    FAIL_IF(size < M_HEADER_SIZE, "score file is shorter than its header");
    FAIL_IF(size > INT32_MAX, "score file is too large");
    SCORE_FILE file = {
        .data = Reader_ReadAlloc(reader, size, &g_HeapAllocator),
        .size = (int32_t)size,
    };
    MUST(Reader_Result(reader));
    const RESULT result = M_Decrypt(&file);
    if (!IS_OK(result)) {
        ScoreFile_Free(&file);
        return result;
    }
    *out = file;
    return OK;
}

void ScoreFile_Free(SCORE_FILE *const file)
{
    if (file->data != nullptr) {
        Allocator_Free(&g_HeapAllocator, file->data, (size_t)file->size);
    }
    *file = (SCORE_FILE) {};
}

void ScoreList_InitDefaults(SCORE_LIST *const list, const int32_t difficulty)
{
    *list = (SCORE_LIST) {};
    for (int32_t i = 0; i < SCORE_LIST_SIZE; i++) {
        HIGH_SCORE entry = {
            .magic = HIGH_SCORE_DEFAULT_MAGIC,
            .score = (uint32_t)(1000000 - i * 100000),
            .difficulty = (uint8_t)difficulty,
            .stage = 1,
        };
        memcpy(entry.name, "Nanashi ", sizeof(entry.name));
        ScoreList_Insert(list, &entry);
    }
}

int32_t ScoreList_Insert(SCORE_LIST *const list, const HIGH_SCORE *const entry)
{
    int32_t rank = 0;
    while (rank < list->count && list->entries[rank].score > entry->score) {
        rank++;
    }
    if (rank >= SCORE_LIST_SIZE) {
        return rank;
    }
    const int32_t moved =
        (list->count < SCORE_LIST_SIZE ? list->count : SCORE_LIST_SIZE - 1)
        - rank;
    memmove(
        &list->entries[rank + 1], &list->entries[rank],
        (size_t)moved * sizeof(HIGH_SCORE));
    list->entries[rank] = *entry;
    if (list->count < SCORE_LIST_SIZE) {
        list->count++;
    }
    return rank;
}

void ScoreFile_LoadHighScores(
    const SCORE_FILE *const file, SCORE_LIST *const list, const int32_t shot,
    const int32_t difficulty)
{
    for (M_CHUNKS chunks = M_FirstChunk(file); M_ReadChunk(&chunks);
         M_NextChunk(&chunks)) {
        READER reader;
        if (chunks.magic != HIGH_SCORE_MAGIC || chunks.version != M_VERSION) {
            continue;
        }
        if (!M_Record(&chunks, M_HIGH_SCORE_SIZE, &reader)) {
            break;
        }
        HIGH_SCORE entry;
        M_ReadHighScore(&reader, &entry);
        if (entry.shot == shot && entry.difficulty == difficulty) {
            ScoreList_Insert(list, &entry);
        }
    }
}

uint32_t ScoreFile_GetHighScore(
    const SCORE_FILE *const file, const int32_t shot, const int32_t difficulty)
{
    SCORE_LIST list = {};
    ScoreFile_LoadHighScores(file, &list, shot, difficulty);
    return list.count != 0 && list.entries[0].score > SCORE_DEFAULT_HIGH_SCORE
        ? list.entries[0].score
        : SCORE_DEFAULT_HIGH_SCORE;
}

void ScoreFile_LoadSpellCards(
    const SCORE_FILE *const file, SPELL_CARD_RECORD cards[SPELL_CARD_COUNT])
{
    for (M_CHUNKS chunks = M_FirstChunk(file); M_ReadChunk(&chunks);
         M_NextChunk(&chunks)) {
        READER reader;
        if (chunks.magic != M_SPELL_CARD_MAGIC || chunks.version != M_VERSION) {
            continue;
        }
        if (!M_Record(&chunks, M_SPELL_CARD_SIZE, &reader)) {
            break;
        }
        SPELL_CARD_RECORD card = {};
        M_ReadHeader(&reader, &card.header);
        card.bonus = Reader_ReadI32(&reader);
        card.index = Reader_ReadU16(&reader);
        card.checksum = Reader_ReadU8(&reader);
        Reader_ReadBytes(&reader, card.history, sizeof(card.history));
        Reader_ReadBytes(&reader, card.name_bytes, sizeof(card.name_bytes));
        card.attempts = Reader_ReadU16(&reader);
        card.captures = Reader_ReadU16(&reader);
        if (card.index >= SPELL_CARD_COUNT) {
            break;
        }
        const uint8_t *const end =
            memchr(card.name_bytes, 0, sizeof(card.name_bytes));
        const size_t length = end != nullptr ? (size_t)(end - card.name_bytes)
                                             : sizeof(card.name_bytes);
        char *name;
        if (Result_Ignore(
                Cp932_Decode((const char *)card.name_bytes, length, &name))) {
            memcpy(card.name, name, strlen(name) + 1);
            Mem_Free(name);
        }
        cards[card.index] = card;
    }
}

void ScoreFile_LoadClears(
    const SCORE_FILE *const file, CLEAR_RECORD clears[SCORE_SHOT_COUNT])
{
    for (int32_t i = 0; i < SCORE_SHOT_COUNT; i++) {
        clears[i] = (CLEAR_RECORD) {
            .header = { .magic = M_CLEAR_MAGIC,
                        .size = M_CLEAR_SIZE,
                        .size2 = M_CLEAR_SIZE,
                        .version = M_VERSION },
            .without_continues = { 1, 1, 1, 1, 1 },
            .with_continues = { 1, 1, 1, 1, 1 },
            .shot = (uint8_t)i,
        };
    }
    for (M_CHUNKS chunks = M_FirstChunk(file); M_ReadChunk(&chunks);
         M_NextChunk(&chunks)) {
        READER reader;
        if (chunks.magic != M_CLEAR_MAGIC || chunks.version != M_VERSION) {
            continue;
        }
        if (!M_Record(&chunks, M_CLEAR_SIZE, &reader)) {
            break;
        }
        CLEAR_RECORD clear;
        M_ReadHeader(&reader, &clear.header);
        Reader_ReadBytes(
            &reader, clear.without_continues, sizeof(clear.without_continues));
        Reader_ReadBytes(
            &reader, clear.with_continues, sizeof(clear.with_continues));
        clear.shot = Reader_ReadU8(&reader);
        clear.reserved = Reader_ReadU8(&reader);
        if (clear.shot >= SCORE_SHOT_COUNT) {
            break;
        }
        clears[clear.shot] = clear;
    }
}

void ScoreFile_LoadPractice(
    const SCORE_FILE *const file,
    PRACTICE_RECORD practice[SCORE_SHOT_COUNT][SCORE_STAGE_COUNT]
                            [SCORE_PRACTICE_DIFFICULTY_COUNT])
{
    PRACTICE_RECORD *const flat = &practice[0][0][0];
    for (int32_t shot = 0; shot < SCORE_SHOT_COUNT; shot++) {
        for (int32_t stage = 0; stage < SCORE_STAGE_COUNT; stage++) {
            for (int32_t difficulty = 0;
                 difficulty < SCORE_PRACTICE_DIFFICULTY_COUNT; difficulty++) {
                practice[shot][stage][difficulty] = (PRACTICE_RECORD) {
                    .header = { .magic = M_PRACTICE_MAGIC,
                                .size = M_PRACTICE_SIZE,
                                .size2 = M_PRACTICE_SIZE,
                                .version = M_VERSION },
                    .shot = (uint8_t)shot,
                    .difficulty = (uint8_t)difficulty,
                    .stage = (uint8_t)stage,
                };
            }
        }
    }
    for (M_CHUNKS chunks = M_FirstChunk(file); M_ReadChunk(&chunks);
         M_NextChunk(&chunks)) {
        READER reader;
        if (chunks.magic != M_PRACTICE_MAGIC || chunks.version != M_VERSION) {
            continue;
        }
        if (!M_Record(&chunks, M_PRACTICE_SIZE, &reader)) {
            break;
        }
        PRACTICE_RECORD record;
        M_ReadHeader(&reader, &record.header);
        record.score = Reader_ReadU32(&reader);
        record.shot = Reader_ReadU8(&reader);
        record.difficulty = Reader_ReadU8(&reader);
        record.stage = Reader_ReadU8(&reader);
        record.reserved = Reader_ReadU8(&reader);
        if (record.shot >= 4 || record.difficulty >= 5 || record.stage >= 7) {
            break;
        }
        const int32_t index =
            record.shot * 24 + record.stage * 4 + record.difficulty;
        if (index < M_PRACTICE_COUNT) {
            flat[index] = record;
        }
    }
}

static void M_WriteHeader(
    WRITER *const writer, const uint32_t magic, const uint16_t size,
    const uint8_t reserved[3])
{
    Writer_WriteU32(writer, magic);
    Writer_WriteU16(writer, size);
    Writer_WriteU16(writer, size);
    Writer_WriteU8(writer, M_VERSION);
    Writer_WriteBytes(writer, reserved, 3);
}

static void M_WriteHighScore(
    WRITER *const writer, const HIGH_SCORE *const entry, const int32_t shot,
    const int32_t difficulty)
{
    Writer_WriteU32(writer, entry->magic);
    Writer_WriteU16(writer, M_HIGH_SCORE_SIZE);
    Writer_WriteU16(writer, M_HIGH_SCORE_SIZE);
    Writer_WriteU8(writer, M_VERSION);
    Writer_WriteU8(writer, 0);
    Writer_WriteBytes(writer, entry->reserved, sizeof(entry->reserved));
    Writer_WriteU32(writer, entry->score);
    Writer_WriteU8(writer, (uint8_t)shot);
    Writer_WriteU8(writer, (uint8_t)difficulty);
    Writer_WriteU8(writer, entry->stage);
    Writer_WriteBytes(writer, entry->name, sizeof(entry->name));
}

static void M_WriteChunks(
    WRITER *const writer,
    const SCORE_LIST lists[SCORE_DIFFICULTY_COUNT][SCORE_SHOT_COUNT],
    const SCORE_RECORDS *const records)
{
    M_WriteHeader(writer, M_TH6K_MAGIC, M_TH6K_SIZE, (const uint8_t[3]) {});
    for (int32_t difficulty = 0; difficulty < SCORE_DIFFICULTY_COUNT;
         difficulty++) {
        for (int32_t shot = 0; shot < SCORE_SHOT_COUNT; shot++) {
            const SCORE_LIST *const list = &lists[difficulty][shot];
            for (int32_t i = 0; i < list->count; i++) {
                if (list->entries[i].magic == HIGH_SCORE_MAGIC) {
                    M_WriteHighScore(
                        writer, &list->entries[i], shot, difficulty);
                }
            }
        }
    }
    for (int32_t i = 0; i < SCORE_SHOT_COUNT; i++) {
        const CLEAR_RECORD *const clear = &records->clears[i];
        M_WriteHeader(
            writer, M_CLEAR_MAGIC, M_CLEAR_SIZE, clear->header.reserved);
        Writer_WriteBytes(
            writer, clear->without_continues, sizeof(clear->without_continues));
        Writer_WriteBytes(
            writer, clear->with_continues, sizeof(clear->with_continues));
        Writer_WriteU8(writer, clear->shot);
        Writer_WriteU8(writer, clear->reserved);
    }
    for (int32_t i = 0; i < SPELL_CARD_COUNT; i++) {
        const SPELL_CARD_RECORD *const card = &records->spell_cards[i];
        if (card->header.magic != M_SPELL_CARD_MAGIC) {
            continue;
        }
        M_WriteHeader(
            writer, M_SPELL_CARD_MAGIC, M_SPELL_CARD_SIZE,
            card->header.reserved);
        Writer_WriteI32(writer, card->bonus);
        Writer_WriteU16(writer, (uint16_t)i);
        Writer_WriteU8(writer, card->checksum);
        Writer_WriteBytes(writer, card->history, sizeof(card->history));
        Writer_WriteBytes(writer, card->name_bytes, sizeof(card->name_bytes));
        Writer_WriteU16(writer, card->attempts);
        Writer_WriteU16(writer, card->captures);
    }
    for (int32_t shot = 0; shot < SCORE_SHOT_COUNT; shot++) {
        for (int32_t stage = 0; stage < SCORE_STAGE_COUNT; stage++) {
            for (int32_t difficulty = 0;
                 difficulty < SCORE_PRACTICE_DIFFICULTY_COUNT; difficulty++) {
                const PRACTICE_RECORD *const record =
                    &records->practice[shot][stage][difficulty];
                if (record->score == 0) {
                    continue;
                }
                Writer_WriteU32(writer, record->header.magic);
                Writer_WriteU16(writer, record->header.size);
                Writer_WriteU16(writer, record->header.size2);
                Writer_WriteU8(writer, record->header.version);
                Writer_WriteBytes(
                    writer, record->header.reserved,
                    sizeof(record->header.reserved));
                Writer_WriteU32(writer, record->score);
                Writer_WriteU8(writer, record->shot);
                Writer_WriteU8(writer, record->difficulty);
                Writer_WriteU8(writer, record->stage);
                Writer_WriteU8(writer, record->reserved);
            }
        }
    }
}

void ScoreFile_Write(
    WRITER *const writer, const SCORE_FILE *const previous,
    const SCORE_LIST lists[SCORE_DIFFICULTY_COUNT][SCORE_SHOT_COUNT],
    const SCORE_RECORDS *const records, const uint8_t key, const uint8_t random)
{
    WRITER plain;
    Writer_OpenBuffer(&plain, &g_HeapAllocator, nullptr);
    Writer_WriteU8(&plain, previous->data != nullptr ? previous->data[0] : 0);
    Writer_WriteU8(&plain, key);
    Writer_WriteU16(&plain, 0);
    Writer_WriteU16(&plain, M_VERSION);
    Writer_WriteU8(&plain, random);
    Writer_WriteU8(&plain, previous->data != nullptr ? previous->data[7] : 0);
    Writer_WriteI32(&plain, M_HEADER_SIZE);
    Writer_WriteU32(&plain, 0);
    Writer_WriteI32(&plain, 0);
    M_WriteChunks(&plain, lists, records);
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&plain, &size);
    Result_Ignore(Writer_Close(&plain));
    data[16] = (uint8_t)size;
    data[17] = (uint8_t)(size >> 8);
    data[18] = (uint8_t)(size >> 16);
    data[19] = (uint8_t)(size >> 24);
    uint16_t checksum = 0;
    for (size_t i = 4; i < size; i++) {
        checksum = (uint16_t)(checksum + data[i]);
    }
    data[2] = (uint8_t)checksum;
    data[3] = (uint8_t)(checksum >> 8);
    uint8_t rolling = data[1];
    for (size_t i = 2; i < size; i++) {
        const uint8_t plain_byte = data[i];
        rolling = M_RotateKey(rolling);
        data[i] ^= rolling;
        rolling = (uint8_t)(rolling + plain_byte);
    }
    Writer_WriteBytes(writer, data, size);
    Allocator_Free(&g_HeapAllocator, data, size);
}
