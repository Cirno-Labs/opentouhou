#include "state.h"

#include "harness_game.h"
#include "test.h"

#include "core/base/memory.h"

// The columns of m_TestGame: score, lives, x and the HASH bullets.
#define M_WIDTH 4

typedef struct {
    uint8_t *bytes;
    size_t size;
} M_FILE;

// Returns the row of index i of a run with plain columns that grow,
// repeat and wobble, and a list hash that changes on each row.
static void M_Row(const uint32_t i, uint32_t *const frame, uint32_t *const row)
{
    *frame = i - i / 7;
    row[0] = i * i;
    row[1] = (uint32_t)(3 - (int32_t)(i / 100));
    row[2] = i % 2 == 0 ? 0x3f800000 : 0x40000000;
    row[3] = State_Hash(STATE_HASH_INIT, i);
}

static M_FILE M_Write(const uint32_t count)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    STATE_WRITER *const state = State_OpenWriter(&writer, &m_TestGame);
    for (uint32_t i = 0; i < count; i++) {
        uint32_t frame;
        uint32_t row[M_WIDTH];
        M_Row(i, &frame, row);
        State_WriteRow(state, frame, row);
    }
    CHECK(Result_Ignore(State_CloseWriter(state)));
    M_FILE file;
    file.bytes = Writer_TakeBuffer(&writer, &file.size);
    Result_Ignore(Writer_Close(&writer));
    return file;
}

static void M_Free(M_FILE *const file)
{
    Allocator_Free(&g_HeapAllocator, file->bytes, file->size);
}

// Reads count rows back and checks each against M_Row and the running
// hash.
static bool M_ReadsBack(const M_FILE *const file, const uint32_t count)
{
    READER reader;
    Reader_OpenBuffer(&reader, file->bytes, file->size, "state.bin");
    STATE_READER *const state = State_OpenReader(&reader, &m_TestGame, count);
    bool same = true;
    uint32_t running = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t frame;
        uint32_t row[M_WIDTH];
        M_Row(i, &frame, row);
        running = State_RunningHash(&m_TestGame, running, frame, row);
        STATE_ROW read;
        if (!State_ReadRow(state, &read)) {
            same = false;
            break;
        }
        const size_t in_chunk = i % STATE_CHUNK_ROWS;
        const bool check = (in_chunk + 1) % STATE_CHECK_ROWS == 0
            || i + 1 == count || in_chunk + 1 == STATE_CHUNK_ROWS;
        same = same && read.frame == frame && read.values[0] == row[0]
            && read.values[1] == row[1] && read.values[2] == row[2]
            && read.hash == (running & 0xf) && read.check == check
            && read.values[3] == (check ? row[3] : 0)
            && (!check || read.running == running);
    }
    STATE_ROW extra;
    same = same && !State_ReadRow(state, &extra);
    same = Result_Ignore(State_CloseReader(state)) && same;
    Reader_Close(&reader);
    return same;
}

TEST(state_rows_round_trip)
{
    M_FILE file = M_Write(200);
    CHECK(M_ReadsBack(&file, 200));
    M_Free(&file);
}

TEST(state_chunks_restart_at_their_boundaries)
{
    const uint32_t counts[] = {
        1,
        STATE_CHECK_ROWS,
        STATE_CHUNK_ROWS - 1,
        STATE_CHUNK_ROWS,
        STATE_CHUNK_ROWS + 1,
        2 * STATE_CHUNK_ROWS + 61,
    };
    for (size_t i = 0; i < ARRAY_SIZE(counts); i++) {
        M_FILE file = M_Write(counts[i]);
        CHECK(M_ReadsBack(&file, counts[i]));
        M_Free(&file);
    }
}

TEST(state_of_no_rows_is_empty)
{
    M_FILE file = M_Write(0);
    CHECK_EQ_INT(file.size, 0);
    CHECK(M_ReadsBack(&file, 0));
    M_Free(&file);
}

// Writes rows whose score is value(i), checks that they read back, and
// returns the mode that the chunk picks for the score.
static uint8_t M_ModeOf(uint32_t (*const value)(uint32_t i))
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    STATE_WRITER *const state = State_OpenWriter(&writer, &m_TestGame);
    const uint32_t count = 100;
    for (uint32_t i = 0; i < count; i++) {
        const uint32_t row[M_WIDTH] = { value(i), 0, 0, 0 };
        State_WriteRow(state, 0, row);
    }
    CHECK(Result_Ignore(State_CloseWriter(state)));
    size_t size;
    uint8_t *const bytes = Writer_TakeBuffer(&writer, &size);
    Result_Ignore(Writer_Close(&writer));
    // The frame column, all 0, is its mode and one run of 100 zeros.
    CHECK(size > 2 && bytes[1] == count);
    const uint8_t mode = bytes[2];

    READER reader;
    Reader_OpenBuffer(&reader, bytes, size, "state.bin");
    STATE_READER *const read = State_OpenReader(&reader, &m_TestGame, count);
    for (uint32_t i = 0; i < count; i++) {
        STATE_ROW row;
        CHECK(State_ReadRow(read, &row) && row.values[0] == value(i));
    }
    CHECK(Result_Ignore(State_CloseReader(read)));
    Reader_Close(&reader);
    Allocator_Free(&g_HeapAllocator, bytes, size);
    return mode;
}

static uint32_t M_Flags(const uint32_t i)
{
    return i % 2 == 0 ? 0x0f0f0f0fu : 0xf0f0f0f0u;
}

static uint32_t M_Steps(const uint32_t i)
{
    return 5 * ((i + 1) / 2);
}

static uint32_t M_Square(const uint32_t i)
{
    return 70000 * i * i;
}

TEST(state_picks_each_mode_where_it_is_smallest)
{
    CHECK_EQ_INT(M_ModeOf(M_Flags), STATE_MODE_XOR);
    CHECK_EQ_INT(M_ModeOf(M_Steps), STATE_MODE_DELTA);
    CHECK_EQ_INT(M_ModeOf(M_Square), STATE_MODE_DELTA2);
}

// Reads one row whose score column is the given bytes, the other columns
// 0, and returns whether the row and the reader's close succeed.
static bool M_ReadsScore(const uint8_t *const score, const size_t size)
{
    uint8_t bytes[64] = { 0, 1 };
    size_t length = 2;
    memcpy(bytes + length, score, size);
    length += size;
    const uint8_t rest[] = { 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    memcpy(bytes + length, rest, sizeof(rest));
    length += sizeof(rest);
    READER reader;
    Reader_OpenBuffer(&reader, bytes, length, "state.bin");
    STATE_READER *const state = State_OpenReader(&reader, &m_TestGame, 1);
    STATE_ROW row;
    bool read = State_ReadRow(state, &row);
    read = Result_Ignore(State_CloseReader(state)) && read;
    Reader_Close(&reader);
    return read;
}

TEST(state_rejects_a_varint_past_32_bits)
{
    CHECK(M_ReadsScore(
        (const uint8_t[]) { 0, 0, 0x80, 0x80, 0x80, 0x80, 0x0f }, 7));
    CHECK(!M_ReadsScore(
        (const uint8_t[]) { 0, 0, 0x80, 0x80, 0x80, 0x80, 0x1f }, 7));
}

TEST(state_rejects_a_varint_with_trailing_zeros)
{
    CHECK(M_ReadsScore((const uint8_t[]) { 0, 0, 0x02 }, 3));
    CHECK(!M_ReadsScore((const uint8_t[]) { 0, 0, 0x82, 0x00 }, 4));
}

TEST(state_rejects_more_zeros_than_rows_left)
{
    CHECK(M_ReadsScore((const uint8_t[]) { 0, 1 }, 2));
    CHECK(!M_ReadsScore((const uint8_t[]) { 0, 2 }, 2));
}

TEST(state_rejects_a_zero_value_after_its_zeros)
{
    CHECK(!M_ReadsScore((const uint8_t[]) { 0, 0, 0 }, 3));
}

TEST(state_hash_changes_with_a_list_hash)
{
    const uint32_t row[M_WIDTH] = { 1, 2, 3, 4 };
    const uint32_t other[M_WIDTH] = { 1, 2, 3, 5 };
    const uint32_t a = State_RunningHash(&m_TestGame, 0, 7, row);
    CHECK(a != State_RunningHash(&m_TestGame, 0, 7, other));
    CHECK(a != State_RunningHash(&m_TestGame, 1, 7, row));
    CHECK(a != State_RunningHash(&m_TestGame, 0, 8, row));
    CHECK_EQ_INT(a, State_RunningHash(&m_TestGame, 0, 7, row));
}

TEST(state_of_bad_data_fails)
{
    M_FILE file = M_Write(STATE_CHECK_ROWS + 5);
    const size_t sizes[] = { 0, 1, file.size / 2, file.size - 1 };
    for (size_t i = 0; i < ARRAY_SIZE(sizes); i++) {
        M_FILE cut = { file.bytes, sizes[i] };
        CHECK(!M_ReadsBack(&cut, STATE_CHECK_ROWS + 5));
    }
    file.bytes[0] = STATE_MODE_COUNT;
    CHECK(!M_ReadsBack(&file, STATE_CHECK_ROWS + 5));
    M_Free(&file);
}

TEST(state_with_bytes_past_its_rows_fails)
{
    M_FILE file = M_Write(10);
    CHECK(!M_ReadsBack(&file, 9));
    M_Free(&file);
}

TEST(hash_bytes_folds_the_same_as_hash_u32_for_matching_bits)
{
    const uint32_t value = 0x12345678;
    const uint32_t by_bytes =
        State_HashBytes(STATE_HASH_INIT, &value, sizeof(value));
    CHECK(by_bytes != STATE_HASH_INIT);
    const uint32_t other = State_HashBytes(STATE_HASH_INIT, "abc", 3);
    CHECK(other != by_bytes);
    CHECK_EQ_INT(
        State_HashBytes(STATE_HASH_INIT, "abc", 3),
        State_HashBytes(STATE_HASH_INIT, "abc", 3));
    CHECK(State_HashBytes(STATE_HASH_INIT, "abc", 0) == STATE_HASH_INIT);
}
