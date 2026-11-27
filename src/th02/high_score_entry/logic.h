#pragma once

// Name entry after a run qualifies for the score table, with the
// not-qualified release/press wait around it.

#include "th02/high_scores.h"
#include "th02/input.h"

typedef struct {
    int32_t score;
    uint8_t stage;
    uint8_t shot_type;
    int16_t year;
    uint8_t day;
    uint8_t month;
} SCORE_RUN;

typedef enum {
    HIGH_SCORE_ENTRY_INPUT,
    HIGH_SCORE_ENTRY_FRAME,
    HIGH_SCORE_ENTRY_WAIT_KEY,
    HIGH_SCORE_ENTRY_COMPLETE,
} HIGH_SCORE_ENTRY_RESULT;

typedef struct {
    HIGH_SCORE_ENTRY_RESULT result;
    bool save_section;
} HIGH_SCORE_ENTRY_EVENTS;

// Returns internal_score * 10 + continues with 32-bit wrap; ending scores
// are already in these units.
int32_t HighScoreEntry_MainScore(int32_t internal_score, uint16_t continues);

// Returns the gaiji at alphabet column 0..16, row 0..2.
uint8_t HighScoreEntry_Alphabet(uint8_t col, uint8_t row);

void HighScoreEntry_Init(void);

// Starts the name entry for a run's score and date; a score that does not
// qualify only waits for release and press.
void HighScoreEntry_Begin(
    HIGH_SCORE_SECTION *section, const SCORE_RUN *run, bool ending,
    const TH02_INPUT *input);

// Runs one frame; a completed entry requests save_section once.
HIGH_SCORE_ENTRY_EVENTS HighScoreEntry_Update(const TH02_INPUT *input);
