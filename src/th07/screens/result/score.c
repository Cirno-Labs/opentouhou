#include "th07/screens/result/score.h"

#include "stg/lzss.h"

#include "core/base/memory.h"
#include "core/base/utils.h"

#include <string.h>

#define M_HEADER_SIZE 0x1c
#define M_FILE_VERSION 0xb
#define M_VERSION 1
// LoadScoreDat unpacks the chunks into a block of this size.
#define M_MAX_DATA_SIZE 0xa0000
#define M_TH7K_MAGIC 0x4b374854
#define M_CLEAR_MAGIC 0x44524c43
#define M_PRACTICE_MAGIC 0x52435350
#define M_LAST_NAME_MAGIC 0x4d4e534c
#define M_PLAY_STATS_MAGIC 0x54534c50
#define M_GAME_VERSION_MAGIC 0x4d535256
#define M_CHUNK_HEADER_SIZE 12
#define M_HIGH_SCORE_SIZE 0x28
#define M_SPELL_CARD_SIZE 0x78
#define M_CLEAR_SIZE 0x1c
#define M_PRACTICE_SIZE 0x18
#define M_LAST_NAME_SIZE 0x18
#define M_PLAY_STATS_SIZE 0x160
#define M_GAME_VERSION_SIZE 0x1c
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
        chunks.position = M_HEADER_SIZE;
        chunks.left = M_I32At(file, 0x10) - M_HEADER_SIZE;
    }
    return chunks;
}

// Reads the header of the chunk at the position into chunks. Returns
// false once the scan ends or the header reaches past the data.
static bool M_ReadChunk(M_CHUNKS *const chunks)
{
    if (chunks->left <= 0
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

// Prepares a reader over the record of size bytes at the chunk when the
// chunk has magic and version 1. Returns false otherwise, and sets *past
// when the record reaches past the data.
static bool M_Record(
    const M_CHUNKS *const chunks, const uint32_t magic, const int32_t size,
    READER *const out, bool *const past)
{
    if (chunks->magic != magic || chunks->version != M_VERSION) {
        return false;
    }
    if (chunks->position > chunks->file->size - size) {
        *past = true;
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

static void M_WriteHeader(
    WRITER *const writer, const SCORE_RECORD_HEADER *const header)
{
    Writer_WriteU32(writer, header->magic);
    Writer_WriteU16(writer, header->size);
    Writer_WriteU16(writer, header->size2);
    Writer_WriteU8(writer, header->version);
    Writer_WriteBytes(writer, header->reserved, sizeof(header->reserved));
}

// Sets the magic, sizes and version that the original sets before it
// copies a record.
static SCORE_RECORD_HEADER M_Header(
    const SCORE_RECORD_HEADER *const header, const uint32_t magic,
    const uint16_t size)
{
    SCORE_RECORD_HEADER out = *header;
    out.magic = magic;
    out.size = size;
    out.size2 = size;
    out.version = M_VERSION;
    return out;
}

static void M_ReadHighScore(READER *const reader, HIGH_SCORE *const out)
{
    out->magic = Reader_ReadU32(reader);
    Reader_Skip(reader, 5);
    out->is_new = Reader_ReadU8(reader) != 0;
    Reader_ReadBytes(reader, out->reserved, sizeof(out->reserved));
    out->score = Reader_ReadU32(reader);
    out->slowdown = Reader_ReadF32(reader);
    out->shot = Reader_ReadU8(reader);
    out->difficulty = Reader_ReadU8(reader);
    out->stage = Reader_ReadU8(reader);
    Reader_ReadBytes(reader, out->name, sizeof(out->name));
    Reader_ReadBytes(reader, out->date, sizeof(out->date));
    out->continues_used = Reader_ReadU8(reader);
    out->reserved_end = Reader_ReadU8(reader);
}

static void M_Decrypt(uint8_t *const data, const int32_t size)
{
    uint8_t key = 0;
    for (int32_t i = 2; i < size; i++) {
        key = M_RotateKey((uint8_t)(key + data[i - 1]));
        data[i] ^= key;
    }
}

static uint16_t M_Checksum(const uint8_t *const data, const int32_t size)
{
    uint16_t checksum = 0;
    for (int32_t i = 4; i < size; i++) {
        checksum = (uint16_t)(checksum + data[i]);
    }
    return checksum;
}

// Returns whether every chunk lies in the data with a size, and a TH7K
// chunk of version 1 is among them, as the original's check after
// unpacking finds.
static bool M_HasValidChunks(const SCORE_FILE *const file)
{
    bool found = false;
    M_CHUNKS chunks = M_FirstChunk(file);
    while (chunks.left > 0) {
        if (!M_ReadChunk(&chunks)) {
            return false;
        }
        if (chunks.magic == M_TH7K_MAGIC) {
            found = chunks.version == M_VERSION;
        }
        M_NextChunk(&chunks);
    }
    return found;
}

RESULT ScoreFile_Read(READER *const reader, SCORE_FILE *const out)
{
    *out = (SCORE_FILE) {};
    const size_t size = Reader_Remaining(reader);
    FAIL_IF(size < M_HEADER_SIZE, "score file is shorter than its header");
    FAIL_IF(size > INT32_MAX, "score file is too large");
    uint8_t *const raw = Reader_ReadAlloc(reader, size, &g_HeapAllocator);
    MUST(Reader_Result(reader));
    M_Decrypt(raw, (int32_t)size);
    const SCORE_FILE packed = { raw, (int32_t)size };
    READER fields = M_ReaderAt(&packed, 2);
    const uint16_t checksum = Reader_ReadU16(&fields);
    const uint16_t version = Reader_ReadU16(&fields);
    Reader_Skip(&fields, 2);
    const int32_t data_offset = Reader_ReadI32(&fields);
    Reader_Skip(&fields, 8);
    const int32_t data_size = Reader_ReadI32(&fields);
    const int32_t packed_size = Reader_ReadI32(&fields);
    RESULT result = OK;
    if (checksum != M_Checksum(raw, (int32_t)size)) {
        result = FAIL("score file checksum does not match");
    } else if (data_offset != M_HEADER_SIZE) {
        result = FAIL("score file header size does not match");
    } else if (version != M_FILE_VERSION) {
        result = FAIL("unsupported score file version 0x%x", version);
    } else if (
        data_size < 0 || data_size > M_MAX_DATA_SIZE || packed_size < 0
        || (size_t)packed_size > size - M_HEADER_SIZE) {
        result = FAIL("score file sizes reach past its data");
    }
    SCORE_FILE file = {};
    if (IS_OK(result)) {
        file.size = M_HEADER_SIZE + data_size;
        file.data = Allocator_Alloc(&g_HeapAllocator, (size_t)file.size);
        memcpy(file.data, raw, M_HEADER_SIZE);
        READER input;
        Reader_OpenBuffer(
            &input, raw + M_HEADER_SIZE, (size_t)packed_size, nullptr);
        size_t decoded;
        result = Lzss_Decode(
            &input, file.data + M_HEADER_SIZE, (size_t)data_size, &decoded,
            nullptr);
        Reader_Close(&input);
        if (IS_OK(result) && decoded != (size_t)data_size) {
            result = FAIL("score file data is truncated");
        }
        if (IS_OK(result) && !M_HasValidChunks(&file)) {
            result = FAIL("score file has an invalid chunk or no TH7K chunk");
        }
    }
    Allocator_Free(&g_HeapAllocator, raw, size);
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
    memset(list, 0, sizeof(*list));
    for (int32_t i = 0; i < SCORE_LIST_SIZE; i++) {
        HIGH_SCORE entry = {
            .magic = HIGH_SCORE_DEFAULT_MAGIC,
            .score = (uint32_t)(100000 - i * 10000),
            .difficulty = (uint8_t)difficulty,
            .stage = 1,
        };
        memcpy(entry.name, "--------", sizeof(entry.name));
        memcpy(entry.date, "--/--", sizeof(entry.date));
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
    bool past = false;
    for (M_CHUNKS chunks = M_FirstChunk(file); !past && M_ReadChunk(&chunks);
         M_NextChunk(&chunks)) {
        READER reader;
        if (!M_Record(
                &chunks, HIGH_SCORE_MAGIC, M_HIGH_SCORE_SIZE, &reader, &past)) {
            continue;
        }
        HIGH_SCORE entry = {};
        M_ReadHighScore(&reader, &entry);
        if (entry.shot == shot && entry.difficulty == difficulty) {
            ScoreList_Insert(list, &entry);
        }
    }
}

uint32_t ScoreFile_GetHighScore(
    const SCORE_FILE *const file, const int32_t shot, const int32_t difficulty,
    uint8_t *const continues)
{
    SCORE_LIST list = {};
    ScoreFile_LoadHighScores(file, &list, shot, difficulty);
    *continues = list.count != 0 ? list.entries[0].continues_used : 0;
    return list.count != 0 && list.entries[0].score > SCORE_DEFAULT_HIGH_SCORE
        ? list.entries[0].score
        : SCORE_DEFAULT_HIGH_SCORE;
}

void ScoreFile_LoadSpellCards(
    const SCORE_FILE *const file, SPELL_CARD_RECORD cards[SPELL_CARD_COUNT])
{
    bool past = false;
    for (M_CHUNKS chunks = M_FirstChunk(file); !past && M_ReadChunk(&chunks);
         M_NextChunk(&chunks)) {
        READER reader;
        if (!M_Record(
                &chunks, SPELL_CARD_MAGIC, M_SPELL_CARD_SIZE, &reader, &past)) {
            continue;
        }
        SPELL_CARD_RECORD card = {};
        M_ReadHeader(&reader, &card.header);
        for (int32_t i = 0; i < 7; i++) {
            card.max_bonus[i] = Reader_ReadU32(&reader);
        }
        card.index = Reader_ReadU16(&reader);
        card.checksum = Reader_ReadU8(&reader);
        Reader_ReadBytes(&reader, card.name, sizeof(card.name));
        for (int32_t i = 0; i < 7; i++) {
            card.attempts[i] = Reader_ReadU16(&reader);
        }
        for (int32_t i = 0; i < 7; i++) {
            card.captures[i] = Reader_ReadU16(&reader);
        }
        if (card.index >= SPELL_CARD_COUNT) {
            break;
        }
        cards[card.index] = card;
    }
}

void ScoreFile_LoadClears(
    const SCORE_FILE *const file, CLEAR_RECORD clears[SCORE_SHOT_COUNT])
{
    for (int32_t i = 0; i < SCORE_SHOT_COUNT; i++) {
        CLEAR_RECORD *const clear = &clears[i];
        memset(clear, 0, sizeof(*clear));
        clear->header = M_Header(&clear->header, M_CLEAR_MAGIC, M_CLEAR_SIZE);
        clear->shot = (uint8_t)i;
        for (int32_t k = 0; k < 5; k++) {
            clear->no_continues[k] = 1;
            clear->with_continues[k] = 1;
        }
    }
    bool past = false;
    for (M_CHUNKS chunks = M_FirstChunk(file); !past && M_ReadChunk(&chunks);
         M_NextChunk(&chunks)) {
        READER reader;
        if (!M_Record(&chunks, M_CLEAR_MAGIC, M_CLEAR_SIZE, &reader, &past)) {
            continue;
        }
        CLEAR_RECORD clear = {};
        M_ReadHeader(&reader, &clear.header);
        Reader_ReadBytes(
            &reader, clear.no_continues, sizeof(clear.no_continues));
        Reader_ReadBytes(
            &reader, clear.with_continues, sizeof(clear.with_continues));
        clear.shot = Reader_ReadU8(&reader);
        Reader_ReadBytes(&reader, clear.reserved, sizeof(clear.reserved));
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
                PRACTICE_RECORD *const record =
                    &practice[shot][stage][difficulty];
                memset(record, 0, sizeof(*record));
                record->header = M_Header(
                    &record->header, M_PRACTICE_MAGIC, M_PRACTICE_SIZE);
                record->shot = (uint8_t)shot;
                record->difficulty = (uint8_t)difficulty;
                record->stage = (uint8_t)stage;
            }
        }
    }
    bool past = false;
    for (M_CHUNKS chunks = M_FirstChunk(file); !past && M_ReadChunk(&chunks);
         M_NextChunk(&chunks)) {
        READER reader;
        if (!M_Record(
                &chunks, M_PRACTICE_MAGIC, M_PRACTICE_SIZE, &reader, &past)) {
            continue;
        }
        PRACTICE_RECORD record = {};
        M_ReadHeader(&reader, &record.header);
        record.play_count = Reader_ReadU32(&reader);
        record.score = Reader_ReadU32(&reader);
        record.shot = Reader_ReadU8(&reader);
        record.difficulty = Reader_ReadU8(&reader);
        record.stage = Reader_ReadU8(&reader);
        record.reserved = Reader_ReadU8(&reader);
        if (record.shot >= 6 || record.difficulty >= 5 || record.stage >= 7) {
            break;
        }
        const int32_t index =
            record.shot * 24 + record.stage * 4 + record.difficulty;
        if (index < M_PRACTICE_COUNT) {
            flat[index] = record;
        }
    }
}

bool ScoreFile_LoadLastName(
    const SCORE_FILE *const file, LAST_NAME_RECORD *const name)
{
    bool past = false;
    for (M_CHUNKS chunks = M_FirstChunk(file); !past && M_ReadChunk(&chunks);
         M_NextChunk(&chunks)) {
        READER reader;
        if (M_Record(
                &chunks, M_LAST_NAME_MAGIC, M_LAST_NAME_SIZE, &reader, &past)) {
            M_ReadHeader(&reader, &name->header);
            Reader_ReadBytes(&reader, name->name, sizeof(name->name));
            return true;
        }
    }
    return false;
}

void ScoreFile_LoadPlayStats(
    const SCORE_FILE *const file, PLAY_STATS_RECORD *const stats)
{
    bool past = false;
    for (M_CHUNKS chunks = M_FirstChunk(file); !past && M_ReadChunk(&chunks);
         M_NextChunk(&chunks)) {
        READER reader;
        if (!M_Record(
                &chunks, M_PLAY_STATS_MAGIC, M_PLAY_STATS_SIZE, &reader,
                &past)) {
            continue;
        }
        M_ReadHeader(&reader, &stats->header);
        uint32_t *const times[] = {
            &stats->run_hours,    &stats->run_minutes,
            &stats->run_seconds,  &stats->run_milliseconds,
            &stats->play_hours,   &stats->play_minutes,
            &stats->play_seconds, &stats->play_milliseconds,
        };
        for (size_t i = 0; i < ARRAY_SIZE(times); i++) {
            *times[i] = Reader_ReadU32(&reader);
        }
        for (int32_t i = 0; i < PLAY_STATS_DIFFICULTY_COUNT; i++) {
            PLAY_COUNTS *const counts = &stats->counts[i];
            for (int32_t shot = 0; shot < 7; shot++) {
                counts->plays[shot] = Reader_ReadI32(&reader);
            }
            counts->retries = Reader_ReadI32(&reader);
            counts->clears = Reader_ReadI32(&reader);
            counts->continues = Reader_ReadI32(&reader);
            counts->practices = Reader_ReadI32(&reader);
        }
    }
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
    Writer_WriteF32(writer, entry->slowdown);
    Writer_WriteU8(writer, (uint8_t)shot);
    Writer_WriteU8(writer, (uint8_t)difficulty);
    Writer_WriteU8(writer, entry->stage);
    Writer_WriteBytes(writer, entry->name, sizeof(entry->name));
    Writer_WriteBytes(writer, entry->date, sizeof(entry->date));
    Writer_WriteU8(writer, entry->continues_used);
    Writer_WriteU8(writer, entry->reserved_end);
}

static void M_WriteSpellCard(
    WRITER *const writer, const SPELL_CARD_RECORD *const card,
    const int32_t index)
{
    const SCORE_RECORD_HEADER header =
        M_Header(&card->header, SPELL_CARD_MAGIC, M_SPELL_CARD_SIZE);
    M_WriteHeader(writer, &header);
    for (int32_t i = 0; i < 7; i++) {
        Writer_WriteU32(writer, card->max_bonus[i]);
    }
    Writer_WriteU16(writer, (uint16_t)index);
    Writer_WriteU8(writer, card->checksum);
    Writer_WriteBytes(writer, card->name, sizeof(card->name));
    for (int32_t i = 0; i < 7; i++) {
        Writer_WriteU16(writer, card->attempts[i]);
    }
    for (int32_t i = 0; i < 7; i++) {
        Writer_WriteU16(writer, card->captures[i]);
    }
}

static void M_WritePlayStats(
    WRITER *const writer, const PLAY_STATS_RECORD *const stats)
{
    M_WriteHeader(writer, &stats->header);
    const uint32_t times[] = {
        stats->run_hours,        stats->run_minutes,       stats->run_seconds,
        stats->run_milliseconds, stats->play_hours,        stats->play_minutes,
        stats->play_seconds,     stats->play_milliseconds,
    };
    for (size_t i = 0; i < ARRAY_SIZE(times); i++) {
        Writer_WriteU32(writer, times[i]);
    }
    for (int32_t i = 0; i < PLAY_STATS_DIFFICULTY_COUNT; i++) {
        const PLAY_COUNTS *const counts = &stats->counts[i];
        for (int32_t shot = 0; shot < 7; shot++) {
            Writer_WriteI32(writer, counts->plays[shot]);
        }
        Writer_WriteI32(writer, counts->retries);
        Writer_WriteI32(writer, counts->clears);
        Writer_WriteI32(writer, counts->continues);
        Writer_WriteI32(writer, counts->practices);
    }
}

static void M_WriteChunks(
    WRITER *const writer,
    const SCORE_LIST lists[SCORE_DIFFICULTY_COUNT][SCORE_SHOT_COUNT],
    const SCORE_RECORDS *const records)
{
    const SCORE_RECORD_HEADER th7k =
        M_Header(&(const SCORE_RECORD_HEADER) {}, M_TH7K_MAGIC, 12);
    M_WriteHeader(writer, &th7k);
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
        const SCORE_RECORD_HEADER header =
            M_Header(&clear->header, M_CLEAR_MAGIC, M_CLEAR_SIZE);
        M_WriteHeader(writer, &header);
        Writer_WriteBytes(
            writer, clear->no_continues, sizeof(clear->no_continues));
        Writer_WriteBytes(
            writer, clear->with_continues, sizeof(clear->with_continues));
        Writer_WriteU8(writer, clear->shot);
        Writer_WriteBytes(writer, clear->reserved, sizeof(clear->reserved));
    }
    for (int32_t i = 0; i < SPELL_CARD_COUNT; i++) {
        if (records->spell_cards[i].header.magic == SPELL_CARD_MAGIC) {
            M_WriteSpellCard(writer, &records->spell_cards[i], i);
        }
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
                M_WriteHeader(writer, &record->header);
                Writer_WriteU32(writer, record->play_count);
                Writer_WriteU32(writer, record->score);
                Writer_WriteU8(writer, record->shot);
                Writer_WriteU8(writer, record->difficulty);
                Writer_WriteU8(writer, record->stage);
                Writer_WriteU8(writer, record->reserved);
            }
        }
    }
    M_WriteHeader(writer, &records->last_name->header);
    Writer_WriteBytes(
        writer, records->last_name->name, sizeof(records->last_name->name));
    M_WritePlayStats(writer, records->play_stats);
    const SCORE_RECORD_HEADER version = M_Header(
        &(const SCORE_RECORD_HEADER) {}, M_GAME_VERSION_MAGIC,
        M_GAME_VERSION_SIZE);
    M_WriteHeader(writer, &version);
    Writer_WriteBytes(writer, "0100b\0\0", 8);
    Writer_WriteU32(writer, 0);
    Writer_WriteU32(writer, 0);
}

static void M_PutI32(uint8_t *const data, const int32_t at, const int32_t value)
{
    for (int32_t i = 0; i < 4; i++) {
        data[at + i] = (uint8_t)((uint32_t)value >> (8 * i));
    }
}

void ScoreFile_Write(
    WRITER *const writer, const SCORE_FILE *const previous,
    const SCORE_LIST lists[SCORE_DIFFICULTY_COUNT][SCORE_SHOT_COUNT],
    const SCORE_RECORDS *const records, const uint8_t key, const uint8_t random)
{
    WRITER plain;
    Writer_OpenBuffer(&plain, &g_HeapAllocator, nullptr);
    M_WriteChunks(&plain, lists, records);
    size_t data_size;
    uint8_t *const chunks = Writer_TakeBuffer(&plain, &data_size);
    Result_Ignore(Writer_Close(&plain));
    WRITER packed;
    Writer_OpenBuffer(&packed, &g_HeapAllocator, nullptr);
    Lzss_Encode(chunks, data_size, &packed);
    Allocator_Free(&g_HeapAllocator, chunks, data_size);
    size_t packed_size;
    uint8_t *const body = Writer_TakeBuffer(&packed, &packed_size);
    Result_Ignore(Writer_Close(&packed));

    const int32_t size = M_HEADER_SIZE + (int32_t)packed_size;
    uint8_t *const data = Allocator_Alloc(&g_HeapAllocator, (size_t)size);
    memset(data, 0, M_HEADER_SIZE);
    // The original copies the header of the file it loaded, and with it
    // the in-memory pointer to its score list at 0xc; the port keeps the
    // loaded file's bytes there instead.
    if (previous->data != nullptr) {
        data[0] = previous->data[0];
        data[7] = previous->data[7];
        memcpy(data + 0xc, previous->data + 0xc, 4);
    }
    data[1] = key;
    data[4] = M_FILE_VERSION;
    data[6] = random;
    M_PutI32(data, 0x8, M_HEADER_SIZE);
    M_PutI32(data, 0x10, M_HEADER_SIZE + (int32_t)data_size);
    M_PutI32(data, 0x14, (int32_t)data_size);
    M_PutI32(data, 0x18, (int32_t)packed_size);
    memcpy(data + M_HEADER_SIZE, body, packed_size);
    Allocator_Free(&g_HeapAllocator, body, packed_size);
    const uint16_t checksum = M_Checksum(data, size);
    data[2] = (uint8_t)checksum;
    data[3] = (uint8_t)(checksum >> 8);
    uint8_t rolling = data[1];
    for (int32_t i = 2; i < size; i++) {
        const uint8_t plain_byte = data[i];
        rolling = M_RotateKey(rolling);
        data[i] ^= rolling;
        rolling = (uint8_t)(rolling + plain_byte);
    }
    Writer_WriteBytes(writer, data, (size_t)size);
    Allocator_Free(&g_HeapAllocator, data, (size_t)size);
}
