// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/arena.h"
#include "core/base/array.h"
#include "core/base/result.h"
#include "core/io/reader.h"
#include "core/io/writer.h"

// Holds one game's replay test case: a header, then a stream of calc rows.
// A capture tool writes a case from a run of the original executable; a
// game's headless runner reads it back and compares its own run against
// the rows. The game decides what the start and calc columns hold; this
// module only reads and writes the format, and does not compare anything.
//
// Every calc row holds one u32 per calc column, one row per 60 Hz calc. An
// f32 is stored as its bits and an i32 as two's complement, so the module
// need not know a column's real type.
//
// Layout, little-endian: CASE_MAGIC, a u16 version, the game id in
// CASE_GAME_ID_SIZE bytes padded with zeros, the info as a u32 length and
// its bytes, the start names, the start row as u32s, the calc names, then
// one zlib stream of calc rows to the end of the file. Names are a u32
// count, then each name as a u8 length and its bytes. Each calc row is
// XORed with the row before it, the first with zeros, before compression.
// A reader accepts only CASE_VERSION.

#define CASE_MAGIC "OTRC"
#define CASE_VERSION 1
#define CASE_GAME_ID_SIZE 8
#define CASE_HASH_INIT 0x811c9dc5u

typedef ARRAY(const char *) CASE_NAMES;
typedef ARRAY(uint32_t) CASE_ROW;

// Holds everything in a case before its calc rows.
typedef struct {
    char game_id[CASE_GAME_ID_SIZE + 1];
    // Holds free text for people: the executable, the source replay.
    const char *info;
    CASE_NAMES start_names;
    CASE_ROW start;
    CASE_NAMES calc_names;
} CASE_HEADER;

typedef struct CASE_WRITER CASE_WRITER;
typedef struct CASE_READER CASE_READER;

// Writes header to out and returns a writer for the calc rows. out must
// stay open until Case_CloseWriter; it is not closed here.
//
// header->start.count not matching header->start_names.count, no calc
// names, a column name that is empty, over 255 bytes or repeated in its
// table, or a game_id longer than CASE_GAME_ID_SIZE, is a programming
// error and is fatal.
CASE_WRITER *Case_OpenWriter(WRITER *out, const CASE_HEADER *header);

// Appends one row of header->calc_names.count values.
void Case_WriteRow(CASE_WRITER *writer, const uint32_t *row);

// Ends the calc row stream, destroys writer, and returns the result of the
// writes to out. Does not close out.
RESULT Case_CloseWriter(CASE_WRITER *writer);

// Reads a case header from in, which holds untrusted data, and returns a
// reader for its calc rows. Every allocation, including the CASE_READER
// itself, comes from arena, so the case and its reader stay valid for as
// long as arena does. On failure, *out is unchanged.
RESULT Case_OpenReader(READER *in, ARENA *arena, CASE_READER **out);

// Returns the header of reader, valid for as long as reader is.
const CASE_HEADER *Case_GetHeader(const CASE_READER *reader);

// Returns the next calc row, valid until the next call to Case_ReadRow, or
// nullptr after the last row. A corrupt or truncated stream, trailing
// bytes after it, or a failure of in, sets a sticky error flag and makes
// every later call return nullptr too.
const uint32_t *Case_ReadRow(CASE_READER *reader);

// Ends decompression and returns the failure that set reader's error flag,
// or OK when it was never set. The memory of reader comes back when its
// arena is reset or freed.
RESULT Case_CloseReader(CASE_READER *reader);

// Returns the index of name in names, or -1 when it is not there.
int32_t Case_FindColumn(const CASE_NAMES *names, const char *name);

// Folds value into hash with FNV-1a, one byte at a time in little-endian
// order. CASE_HASH_INIT is the FNV-1a initial value.
uint32_t Case_Hash(uint32_t hash, uint32_t value);
