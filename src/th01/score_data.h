#pragma once

#include "core/io/reader.h"
#include "core/io/writer.h"
#include "core/vfs.h"

#define SCORE_PLACES 10
#define SCORE_RANKS 4
#define SCORE_NAME_SIZE 65
#define SCORE_FILE_SIZE 247

typedef struct {
    char name[SCORE_NAME_SIZE], route[9];
    int32_t score;
    int16_t stage;
} SCORE_ENTRY;

typedef struct {
    SCORE_ENTRY entries[SCORE_PLACES];
} SCORE_TABLE;

typedef struct {
    SCORE_TABLE tables[SCORE_RANKS];
    bool pending[SCORE_RANKS], writable[SCORE_RANKS];
    VFS *user_files;
} SCORE_SAVES;

// Fills table with the original's ten default entries.
void ScoreData_Defaults(SCORE_TABLE *table);

// Reads one original record. Checks only the first four signature bytes, as
// the original does, and accepts trailing bytes. Rejects invalid CP932.
// Names keep all sixteen original bytes, including ASCII spaces.
RESULT ScoreData_Read(READER *reader, SCORE_TABLE *out);
RESULT ScoreData_Write(WRITER *writer, const SCORE_TABLE *table);

// Inserts entry before equal scores and returns its place, or SCORE_PLACES
// when the score does not qualify. The name must already encode to sixteen
// CP932 bytes.
int32_t ScoreData_Insert(SCORE_TABLE *table, const SCORE_ENTRY *entry);
const char *ScoreData_File(int32_t rank);

// Loads each rank from user_files, else from original, else the defaults,
// queueing ranks missing in user_files for creation. Never writes original
// files. Disables writes for a rank whose file in user_files is invalid, to
// keep it. Reports invalid files while leaving all four tables usable.
RESULT ScoreData_Load(VFS *original, VFS *user_files, SCORE_SAVES *out);

// Queues a rank for ScoreData_WriteSaves.
void ScoreData_Queue(SCORE_SAVES *saves, int32_t rank);

// Writes the queued tables to user_files atomically, clearing the queue even
// on failure.
RESULT ScoreData_WriteSaves(SCORE_SAVES *saves);
