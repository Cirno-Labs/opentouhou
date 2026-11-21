#pragma once

#include "game.h"

#include "core/archive.h"
#include "core/archive/zip.h"
#include "core/base/arena.h"
#include "core/base/result.h"
#include "core/io/reader.h"
#include "core/io/writer.h"

// Reads and writes a case: one ZIP file of case.ini, the manifest,
// inputs.txt (script.h), state.bin (state.h) and the files that
// its staging steps serve among the game's files, each at files/NAME.

#define CASE_FORMAT 6

// Serves the case entry source, files/NAME, at path among the game's files
// from frame 0 on. path is in the normal form of core/vfs.
typedef struct {
    const char *source;
    const char *path;
} STAGE_STEP;

typedef struct {
    // Holds CASE_FORMAT.
    uint32_t format;
    const char *game;
    const char *game_version;
    // Names the backend that captured the case.
    const char *backend;
    uint32_t columns_version;
    uint32_t frames;
    // Counts the rows of state.bin.
    uint32_t rows;
    // Holds one value per choice of the game, from arena.
    uint32_t *choices;
    // Holds free text for people: the source replay, the player.
    const char *note;
    // Lists the staging steps in the order that a run takes them.
    const STAGE_STEP *steps;
    size_t step_count;
    // Lists the start values of [start], in the order of the game's start
    // fields.
    const START_VALUE *starts;
    size_t start_count;
} CASE_MANIFEST;

// Checks step: source is files/ and one name, and path is in the normal
// form of core/vfs.
RESULT Case_CheckStep(const STAGE_STEP *step);

// Reads case.ini, with the choices and start fields of game. Everything
// comes from arena. Reports failure for any format but CASE_FORMAT, for a
// missing choice, a bad start value and a bad staging step. On failure, leaves
// out unchanged.
RESULT Case_ReadManifest(
    READER *in, const HARNESS_GAME *game, ARENA *arena, CASE_MANIFEST *out);

// Writes manifest, with the choices of game, as case.ini.
void Case_WriteManifest(
    WRITER *out, const HARNESS_GAME *game, const CASE_MANIFEST *manifest);

// Reads the entry name of the case file archive whole into memory from the
// archive's allocator. Reports failure when it is missing.
RESULT Case_ReadEntry(ARCHIVE *archive, const char *name, ZIP_ENTRY *out);
