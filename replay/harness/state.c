#include "state.h"

#include "core/base/memory.h"

// Holds the indices in a row, the frame being 0, of the values that a chunk
// stores and of the game's HASH columns.
typedef struct {
    size_t width;
    size_t *stored;
    size_t stored_count;
    size_t *hashes;
    size_t hash_count;
} M_LAYOUT;

struct STATE_WRITER {
    WRITER *out;
    const HARNESS_GAME *game;
    M_LAYOUT layout;
    // Holds the rows of the chunk, each the frame and then the columns.
    uint32_t *rows;
    uint32_t *running;
    size_t count;
    uint32_t last_running;
};

struct STATE_READER {
    READER *in;
    M_LAYOUT layout;
    uint32_t *rows;
    uint8_t *hashes;
    uint32_t *checks;
    uint64_t left;
    size_t count;
    size_t next;
    bool bad;
};

static void M_InitLayout(M_LAYOUT *const layout, const HARNESS_GAME *game)
{
    *layout = (M_LAYOUT) { .width = game->column_count + 1 };
    layout->stored = Mem_AllocArrZ(game->column_count + 1, sizeof(size_t));
    layout->hashes = Mem_AllocArrZ(game->column_count, sizeof(size_t));
    layout->stored[layout->stored_count++] = 0;
    for (size_t i = 0; i < game->column_count; i++) {
        if (game->columns[i].kind == COLUMN_HASH) {
            layout->hashes[layout->hash_count++] = i + 1;
        } else {
            layout->stored[layout->stored_count++] = i + 1;
        }
    }
}

static void M_FreeLayout(M_LAYOUT *const layout)
{
    Mem_Free(layout->stored);
    Mem_Free(layout->hashes);
}

static bool M_IsCheck(const size_t row, const size_t count)
{
    return (row + 1) % STATE_CHECK_ROWS == 0 || row + 1 == count;
}

static size_t M_CheckCount(const size_t count)
{
    return (count + STATE_CHECK_ROWS - 1) / STATE_CHECK_ROWS;
}

uint32_t State_RunningHash(
    const HARNESS_GAME *const game, const uint32_t before, const uint32_t frame,
    const uint32_t *const row)
{
    uint32_t hash = State_Hash(STATE_HASH_INIT, before);
    hash = State_Hash(hash, frame);
    for (size_t i = 0; i < game->column_count; i++) {
        hash = State_Hash(hash, row[i]);
    }
    return hash;
}

static uint32_t M_Encode(
    const STATE_MODE mode, const uint32_t value, const uint32_t a,
    const uint32_t b)
{
    switch (mode) {
    case STATE_MODE_DELTA:
        return value - a;
    case STATE_MODE_DELTA2:
        return value - 2 * a + b;
    case STATE_MODE_XOR:
    case STATE_MODE_COUNT:
        break;
    }
    return value ^ a;
}

static uint32_t M_Decode(
    const STATE_MODE mode, const uint32_t encoded, const uint32_t a,
    const uint32_t b)
{
    switch (mode) {
    case STATE_MODE_DELTA:
        return encoded + a;
    case STATE_MODE_DELTA2:
        return encoded + 2 * a - b;
    case STATE_MODE_XOR:
    case STATE_MODE_COUNT:
        break;
    }
    return encoded ^ a;
}

static uint32_t M_Zigzag(const uint32_t value)
{
    return value << 1 ^ (uint32_t)-(int32_t)(value >> 31);
}

static uint32_t M_Unzigzag(const uint32_t value)
{
    return value >> 1 ^ (uint32_t)-(int32_t)(value & 1);
}

static size_t M_VarintSize(uint32_t value)
{
    size_t size = 1;
    while (value >= 0x80) {
        value >>= 7;
        size++;
    }
    return size;
}

static void M_WriteVarint(WRITER *const out, uint32_t value)
{
    while (value >= 0x80) {
        Writer_WriteU8(out, (uint8_t)(value | 0x80));
        value >>= 7;
    }
    Writer_WriteU8(out, (uint8_t)value);
}

// Returns the value of column of the writer's row, 0 before the first.
static uint32_t M_Value(
    const STATE_WRITER *const writer, const size_t row, const size_t back,
    const size_t column)
{
    return row >= back
        ? writer->rows[(row - back) * writer->layout.width + column]
        : 0;
}

// Encodes column in mode and writes it to out, or with out nullptr only
// returns its size.
static size_t M_WriteColumn(
    const STATE_WRITER *const writer, const size_t column,
    const STATE_MODE mode, WRITER *const out)
{
    size_t size = 0;
    uint32_t zeros = 0;
    for (size_t row = 0; row < writer->count; row++) {
        const uint32_t encoded = M_Encode(
            mode, M_Value(writer, row, 0, column),
            M_Value(writer, row, 1, column), M_Value(writer, row, 2, column));
        if (encoded == 0) {
            zeros++;
            continue;
        }
        const uint32_t zigzag = M_Zigzag(encoded);
        size += M_VarintSize(zeros) + M_VarintSize(zigzag);
        if (out != nullptr) {
            M_WriteVarint(out, zeros);
            M_WriteVarint(out, zigzag);
        }
        zeros = 0;
    }
    if (zeros > 0) {
        size += M_VarintSize(zeros);
        if (out != nullptr) {
            M_WriteVarint(out, zeros);
        }
    }
    return size;
}

static void M_Flush(STATE_WRITER *const writer)
{
    const M_LAYOUT *const layout = &writer->layout;
    if (writer->count == 0) {
        return;
    }
    for (size_t i = 0; i < layout->stored_count; i++) {
        const size_t column = layout->stored[i];
        STATE_MODE best = STATE_MODE_XOR;
        size_t best_size = SIZE_MAX;
        for (STATE_MODE mode = 0; mode < STATE_MODE_COUNT; mode++) {
            const size_t size = M_WriteColumn(writer, column, mode, nullptr);
            if (size < best_size) {
                best = mode;
                best_size = size;
            }
        }
        Writer_WriteU8(writer->out, (uint8_t)best);
        M_WriteColumn(writer, column, best, writer->out);
    }
    for (size_t row = 0; row < writer->count; row += 2) {
        const uint32_t high =
            row + 1 < writer->count ? writer->running[row + 1] & 0xf : 0;
        Writer_WriteU8(
            writer->out, (uint8_t)((writer->running[row] & 0xf) | high << 4));
    }
    for (size_t row = 0; row < writer->count; row++) {
        if (!M_IsCheck(row, writer->count)) {
            continue;
        }
        Writer_WriteU32(writer->out, writer->running[row]);
        for (size_t i = 0; i < layout->hash_count; i++) {
            Writer_WriteU32(
                writer->out,
                writer->rows[row * layout->width + layout->hashes[i]]);
        }
    }
    writer->count = 0;
}

STATE_WRITER *State_OpenWriter(
    WRITER *const out, const HARNESS_GAME *const game)
{
    STATE_WRITER *const writer = Mem_AllocZ(sizeof(*writer));
    writer->out = out;
    writer->game = game;
    M_InitLayout(&writer->layout, game);
    writer->rows = Mem_AllocArrZ(
        STATE_CHUNK_ROWS * writer->layout.width, sizeof(uint32_t));
    writer->running = Mem_AllocArrZ(STATE_CHUNK_ROWS, sizeof(uint32_t));
    return writer;
}

void State_WriteRow(
    STATE_WRITER *const writer, const uint32_t frame, const uint32_t *const row)
{
    const size_t width = writer->layout.width;
    uint32_t *const slot = &writer->rows[writer->count * width];
    slot[0] = frame;
    memcpy(slot + 1, row, (width - 1) * sizeof(uint32_t));
    writer->last_running =
        State_RunningHash(writer->game, writer->last_running, frame, row);
    writer->running[writer->count++] = writer->last_running;
    if (writer->count == STATE_CHUNK_ROWS) {
        M_Flush(writer);
    }
}

RESULT State_CloseWriter(STATE_WRITER *const writer)
{
    M_Flush(writer);
    const RESULT result = Writer_Result(writer->out);
    M_FreeLayout(&writer->layout);
    Mem_Free(writer->rows);
    Mem_Free(writer->running);
    Mem_Free(writer);
    return result;
}

STATE_READER *State_OpenReader(
    READER *const in, const HARNESS_GAME *const game, const uint64_t rows)
{
    STATE_READER *const reader = Mem_AllocZ(sizeof(*reader));
    reader->in = in;
    reader->left = rows;
    M_InitLayout(&reader->layout, game);
    reader->rows = Mem_AllocArrZ(
        STATE_CHUNK_ROWS * reader->layout.width, sizeof(uint32_t));
    reader->hashes = Mem_AllocZ((STATE_CHUNK_ROWS + 1) / 2);
    reader->checks = Mem_AllocArrZ(
        M_CheckCount(STATE_CHUNK_ROWS) * (reader->layout.hash_count + 1),
        sizeof(uint32_t));
    return reader;
}

static bool M_ReadVarint(STATE_READER *const reader, uint32_t *const out)
{
    uint32_t value = 0;
    for (int32_t shift = 0; shift < 35; shift += 7) {
        const uint8_t byte = Reader_ReadU8(reader->in);
        if (Reader_Failed(reader->in) || (shift == 28 && byte > 0x0f)) {
            return false;
        }
        value |= (uint32_t)(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) {
            // A value has one encoding, without trailing zero bytes.
            *out = value;
            return byte != 0 || shift == 0;
        }
    }
    return false;
}

static bool M_ReadColumn(STATE_READER *const reader, const size_t column)
{
    const size_t width = reader->layout.width;
    const uint8_t mode = Reader_ReadU8(reader->in);
    if (Reader_Failed(reader->in) || mode >= STATE_MODE_COUNT) {
        return false;
    }
    uint32_t a = 0;
    uint32_t b = 0;
    for (size_t row = 0; row < reader->count;) {
        uint32_t zeros;
        uint32_t zigzag = 0;
        if (!M_ReadVarint(reader, &zeros) || zeros > reader->count - row) {
            return false;
        }
        const size_t end = row + zeros;
        const bool last = end == reader->count;
        if (!last && (!M_ReadVarint(reader, &zigzag) || zigzag == 0)) {
            return false;
        }
        for (; row < end + !last; row++) {
            const uint32_t encoded = row < end ? 0 : M_Unzigzag(zigzag);
            const uint32_t value = M_Decode(mode, encoded, a, b);
            reader->rows[row * width + column] = value;
            b = a;
            a = value;
        }
    }
    return true;
}

static bool M_ReadChunk(STATE_READER *const reader)
{
    const M_LAYOUT *const layout = &reader->layout;
    reader->count = reader->left < STATE_CHUNK_ROWS ? (size_t)reader->left
                                                    : STATE_CHUNK_ROWS;
    reader->next = 0;
    memset(reader->rows, 0, reader->count * layout->width * sizeof(uint32_t));
    for (size_t i = 0; i < layout->stored_count; i++) {
        if (!M_ReadColumn(reader, layout->stored[i])) {
            return false;
        }
    }
    Reader_ReadBytes(reader->in, reader->hashes, (reader->count + 1) / 2);
    const size_t checks =
        M_CheckCount(reader->count) * (layout->hash_count + 1);
    for (size_t i = 0; i < checks; i++) {
        reader->checks[i] = Reader_ReadU32(reader->in);
    }
    return !Reader_Failed(reader->in);
}

bool State_ReadRow(STATE_READER *const reader, STATE_ROW *const out)
{
    const M_LAYOUT *const layout = &reader->layout;
    if (reader->bad || (reader->next == reader->count && reader->left == 0)) {
        return false;
    }
    if (reader->next == reader->count && !M_ReadChunk(reader)) {
        reader->bad = true;
        return false;
    }
    const size_t row = reader->next++;
    reader->left--;
    uint32_t *const values = &reader->rows[row * layout->width];
    const uint8_t pair = reader->hashes[row / 2];
    *out = (STATE_ROW) {
        .frame = values[0],
        .values = values + 1,
        .hash = row % 2 == 0 ? pair & 0xf : pair >> 4,
        .check = M_IsCheck(row, reader->count),
    };
    if (out->check) {
        const uint32_t *const check =
            &reader->checks[row / STATE_CHECK_ROWS * (layout->hash_count + 1)];
        out->running = check[0];
        for (size_t i = 0; i < layout->hash_count; i++) {
            values[layout->hashes[i]] = check[i + 1];
        }
    }
    return true;
}

RESULT State_CloseReader(STATE_READER *const reader)
{
    const bool bad = reader->bad
        || (reader->left == 0 && reader->next == reader->count
            && Reader_Remaining(reader->in) != 0);
    M_FreeLayout(&reader->layout);
    Mem_Free(reader->rows);
    Mem_Free(reader->hashes);
    Mem_Free(reader->checks);
    Mem_Free(reader);
    FAIL_IF(bad, "state.bin does not hold the rows of case.ini");
    return OK;
}
