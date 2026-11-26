#pragma once

// The high-score file HUUHI.DAT: five sections, one per difficulty and
// Extra, each with checksums, and the clear markers that unlock Extra.

#include "core/io/reader.h"
#include "core/io/writer.h"

#define SCORES_SECTIONS 5
#define SCORES_PLACES 10
#define SCORES_NAME_GLYPHS 7
#define SCORES_SECTION_SIZE 182

typedef struct {
    int32_t score;
    uint8_t name_glyphs[SCORES_NAME_GLYPHS];
    uint8_t stage, shot_type, day, month;
    int16_t year;
} SCORE_RECORD;

typedef struct {
    int16_t cleared;
    SCORE_RECORD records[SCORES_PLACES];
} SCORE_SECTION;

typedef struct {
    SCORE_SECTION sections[SCORES_SECTIONS];
} SCORES;

typedef struct {
    bool game[3], extra[3], extra_unlocked;
} SCORE_CLEARS;

// Fills the factory tables, names of six bullet glyphs dated 1900-01-01,
// with no clears.
void Scores_Defaults(SCORES *scores);

// Reads one section, checking its checksums but not its field ranges.
RESULT Scores_ReadSection(READER *reader, SCORE_SECTION *out);

// Writes one section with fresh checksums.
RESULT Scores_WriteSection(WRITER *writer, const SCORE_SECTION *section);

// Reads all five sections, Extra included; failure leaves out unchanged.
RESULT Scores_Read(READER *reader, SCORES *out);
RESULT Scores_Write(WRITER *writer, const SCORES *scores);

// Returns the clears; Extra unlocks only with markers 318/118/218 in
// sections 0..2, and Extra clears are bits 0..2 of section 3.
void Scores_GetClears(const SCORES *scores, SCORE_CLEARS *out);

// Records a clear and returns its section index, or -1 for a normal clear
// with continues.
int8_t Scores_RecordClear(
    SCORES *scores, uint8_t shot_type, bool extra, uint16_t continues_used);
