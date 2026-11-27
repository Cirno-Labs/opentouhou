#pragma once

#include "replay/harness/game.h"

#include "core/base/result.h"
#include "core/io/reader.h"
#include "core/io/writer.h"

#include <stdint.h>
#include <string.h>

// Writes and reads state.bin, a case's rows, one per calc. A row is the
// frame of its calc, then one value per column of the game, each a u32:
// an I32 as two's complement and an F32 as its bits.
//
// The rows lie in chunks of STATE_CHUNK_ROWS, the last one shorter, and
// each chunk decodes on its own. A chunk holds, for the frame and then
// each column but the HASH ones in the game's order, one byte of mode
// (STATE_MODE), then pairs of a varint count of rows whose encoded value
// is 0 and the zigzag varint of the next value, up to the chunk's end,
// where the last pair may lack its value. A row's encoded value is its
// value against the rows before it in the chunk, 0 before the first. A
// varint holds 7 bits per byte, the low ones first, in its shortest form.
//
// The running hash folds, with State_Hash from STATE_HASH_INIT, the hash
// of the row before, 0 before the first, then the frame and every column
// of the row, HASH ones too. After the columns, a chunk holds the low 4
// bits of each row's running hash, two rows per byte, the first in the
// low bits. Then come the checks: after every STATE_CHECK_ROWS rows of a
// chunk and after its last row, the u32 running hash of that row and then
// its HASH columns, each little-endian.
//
// The file holds neither column names nor its row count: case.ini's
// columns_version tells whether its columns are the game's, and its rows
// how many it holds.

#define STATE_CHUNK_ROWS 3600
#define STATE_CHECK_ROWS 60

// Starts a hash's FNV-1a fold.
#define STATE_HASH_INIT 0x811c9dc5u

// Gives what a column's encoded value holds against the values a and b
// of the two rows before.
typedef enum {
    // value ^ a
    STATE_MODE_XOR,
    // value - a
    STATE_MODE_DELTA,
    // value - 2a + b
    STATE_MODE_DELTA2,
    STATE_MODE_COUNT,
} STATE_MODE;

typedef struct STATE_WRITER STATE_WRITER;
typedef struct STATE_READER STATE_READER;

// Gives one row that the reader read.
typedef struct {
    uint32_t frame;
    // Holds one value per column of the game. A HASH column holds its value
    // on a check row and 0 on any other.
    const uint32_t *values;
    // Holds the low 4 bits of the running hash.
    uint8_t hash;
    // Tells whether the row has a check, and then gives its running hash.
    bool check;
    uint32_t running;
} STATE_ROW;

// Folds value into hash with FNV-1a, one byte at a time, little-endian.
static inline uint32_t State_Hash(uint32_t hash, const uint32_t value)
{
    for (int32_t i = 0; i < 4; i++) {
        hash = (hash ^ ((value >> (i * 8)) & 0xff)) * 0x01000193u;
    }
    return hash;
}

// Folds bytes into hash with FNV-1a.
static inline uint32_t State_HashBytes(
    uint32_t hash, const void *const data, const size_t size)
{
    const uint8_t *const bytes = data;
    for (size_t i = 0; i < size; i++) {
        hash = (hash ^ bytes[i]) * 0x01000193u;
    }
    return hash;
}

// Returns the bits of value, which a column of kind F32 holds.
static inline uint32_t State_F32(const float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// Returns the running hash of a row, given before, the running hash of the
// row before it.
uint32_t State_RunningHash(
    const HARNESS_GAME *game, uint32_t before, uint32_t frame,
    const uint32_t *row);

// Returns a writer for the rows of game, which holds one chunk at a time.
// out must stay open until State_CloseWriter.
STATE_WRITER *State_OpenWriter(WRITER *out, const HARNESS_GAME *game);

// Appends one row: frame, then one value per column of the game.
void State_WriteRow(STATE_WRITER *writer, uint32_t frame, const uint32_t *row);

// Writes the last chunk, destroys writer and returns the result of the
// writes to out.
RESULT State_CloseWriter(STATE_WRITER *writer);

// Returns a reader of rows rows of game from in, untrusted data, which
// must stay open until State_CloseReader. The reader holds one chunk at a
// time.
STATE_READER *State_OpenReader(
    READER *in, const HARNESS_GAME *game, uint64_t rows);

// Reads the next row into out, valid until the next call. Returns false
// after the last row or on bad data.
bool State_ReadRow(STATE_READER *reader, STATE_ROW *out);

// Destroys reader. Reports failure when the data was bad or had bytes
// past the last row.
RESULT State_CloseReader(STATE_READER *reader);
