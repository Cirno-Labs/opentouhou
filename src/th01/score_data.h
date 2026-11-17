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

// Creates the source's ten default names, scores, stages and routes.
void ScoreData_Defaults(SCORE_TABLE *table);

// Reads one original record. Accepts the source's four-byte signature check
// and trailing bytes. Rejects truncation and invalid CP932 without changing
// out. Names retain all sixteen original bytes, including ASCII spaces.
RESULT ScoreData_Read(READER *reader, SCORE_TABLE *out);
RESULT ScoreData_Write(WRITER *writer, const SCORE_TABLE *table);

// Inserts before equal signed scores. Returns ten when the score does not
// qualify. The entry name must already be valid original-format text.
int32_t ScoreData_Insert(SCORE_TABLE *table, const SCORE_ENTRY *entry);
const char *ScoreData_File(int32_t rank);

// Loads user files before optional original imports, otherwise uses defaults.
// Never writes original files. Invalid existing user files disable writes
// for that rank to preserve them. user_files is borrowed until saves expire.
// Reports invalid files while leaving all four tables usable.
RESULT ScoreData_Load(VFS *original, VFS *user_files, SCORE_SAVES *out);

// Queues a rank without allocation or IO. Drain after the simulation step.
void ScoreData_Queue(SCORE_SAVES *saves, int32_t rank);

// Writes queued tables atomically in user_files, clearing requests
// even on failure. IO results never modify the simulated tables.
RESULT ScoreData_WriteSaves(SCORE_SAVES *saves);
