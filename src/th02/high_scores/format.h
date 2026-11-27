#pragma once

// The high-score file HUUHI.DAT: five sections, one per difficulty and
// Extra, each with checksums, and the clear markers that unlock Extra.

#include "core/io/reader.h"
#include "core/io/writer.h"

#define HIGH_SCORES_SECTIONS 5
#define HIGH_SCORES_PLACES 10
#define HIGH_SCORES_NAME_GLYPHS 7
#define HIGH_SCORES_SECTION_SIZE 182

typedef struct {
    int32_t score;
    uint8_t name_glyphs[HIGH_SCORES_NAME_GLYPHS];
    uint8_t stage, shot_type, day, month;
    int16_t year;
} HIGH_SCORE_RECORD;

typedef struct {
    int16_t cleared;
    HIGH_SCORE_RECORD records[HIGH_SCORES_PLACES];
} HIGH_SCORE_SECTION;

typedef struct {
    HIGH_SCORE_SECTION sections[HIGH_SCORES_SECTIONS];
} HIGH_SCORES;

typedef struct {
    bool game[3], extra[3], extra_unlocked;
} HIGH_SCORE_CLEARS;

// Fills the factory tables, names of six bullet glyphs dated 1900-01-01,
// with no clears.
void HighScores_Defaults(HIGH_SCORES *scores);

// Reads one section, checking its checksums but not its field ranges.
RESULT HighScores_ReadSection(READER *reader, HIGH_SCORE_SECTION *out);

// Writes one section with fresh checksums.
RESULT HighScores_WriteSection(
    WRITER *writer, const HIGH_SCORE_SECTION *section);

// Reads all five sections, Extra included; failure leaves out unchanged.
RESULT HighScores_Read(READER *reader, HIGH_SCORES *out);
RESULT HighScores_Write(WRITER *writer, const HIGH_SCORES *scores);

// Returns the clears; Extra unlocks only with markers 318/118/218 in
// sections 0..2, and Extra clears are bits 0..2 of section 3.
void HighScores_GetClears(const HIGH_SCORES *scores, HIGH_SCORE_CLEARS *out);

// Records a clear and returns its section index, or -1 for a normal clear
// with continues.
int8_t HighScores_RecordClear(
    HIGH_SCORES *scores, uint8_t shot_type, bool extra,
    uint16_t continues_used);
