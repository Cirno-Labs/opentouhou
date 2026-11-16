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
// The original stores at most 32 tunes and writes longer fields past
// their buffers. The port stops at 32 tunes and cuts a field at its
// buffer: 63 bytes of a path, 33 of a title and 65 of a comment row.

#define MUSIC_TUNE_COUNT 0x20
#define MUSIC_COMMENT_ROWS 8
#define MUSIC_PATH_SIZE 0x40
#define MUSIC_ROW_BYTES 0x42

typedef struct {
    char path[MUSIC_PATH_SIZE]; // as the data names it, such as
                                // "bgm/th07_01.mid"
    char title[MUSIC_ROW_BYTES * 3]; // UTF-8
    char comment[MUSIC_COMMENT_ROWS][MUSIC_ROW_BYTES * 3]; // UTF-8
} MUSIC_TUNE;

// Parses the tunes of reader into tunes and sets *count to their number.
// Reports failure only when the reader fails.
RESULT MusicScreen_ReadComments(
    READER *reader, MUSIC_TUNE tunes[MUSIC_TUNE_COUNT], int32_t *count);
