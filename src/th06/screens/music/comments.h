// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/result.h"
#include "core/io/reader.h"

#include <stdint.h>

// Reads data/musiccmt.txt, the tunes of the music room. Each tune starts
// with a line "@path", then a title line and comment lines. The original
// takes the line break after the title as the first comment row, so the
// first row is always empty and the next seven lines fill the others.
// Text that follows them belongs to no tune.
//
// The music room shows each comment row in two halves of 32 CP932 bytes,
// the second only when the row is longer than 32 bytes. The halves are
// split by bytes, before the conversion to UTF-8, as the original splits
// them. A half that does not convert, such as one that splits a
// character, is empty. A half that is valid UTF-8, as a file from a patch
// can hold, loads as UTF-8.
//
// The original stores at most 32 tunes and writes longer fields past their
// buffers. The port stops at 32 tunes and cuts a field at its buffer: 63
// bytes of a path, 33 of a title and 65 of a comment row.

#define MUSIC_TUNE_COUNT 0x20
#define MUSIC_COMMENT_ROWS 8
#define MUSIC_HALF_BYTES 0x20
#define MUSIC_PATH_SIZE 0x40
#define MUSIC_TITLE_BYTES 0x22

typedef struct {
    char path[MUSIC_PATH_SIZE]; // as the data names it, such as
                                // "bgm/th06_01.mid"
    char title[MUSIC_TITLE_BYTES * 3]; // UTF-8
    // Holds the two halves of each comment row in UTF-8.
    char comment[MUSIC_COMMENT_ROWS][2][MUSIC_HALF_BYTES * 3 + 1];
} MUSIC_TUNE;

// Parses the tunes of reader into tunes and sets *count to their number.
// Reports failure only when the reader fails.
RESULT MusicScreen_ReadComments(
    READER *reader, MUSIC_TUNE tunes[MUSIC_TUNE_COUNT], int32_t *count);
