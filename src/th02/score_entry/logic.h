#pragma once

// Name entry after a run qualifies for the score table, with the
// not-qualified release/press wait around it.

#include "th02/input.h"
#include "th02/scores.h"

typedef struct {
    int32_t score;
    uint8_t stage;
    uint8_t shot_type;
    int16_t year;
    uint8_t day;
    uint8_t month;
} SCORE_RUN;

typedef enum {
    SCORE_ENTRY_INPUT,
    SCORE_ENTRY_FRAME,
    SCORE_ENTRY_WAIT_KEY,
    SCORE_ENTRY_COMPLETE,
} SCORE_ENTRY_RESULT;

typedef struct {
    SCORE_ENTRY_RESULT result;
    bool save_section;
} SCORE_ENTRY_EVENTS;

// Returns internal_score * 10 + continues with 32-bit wrap; ending scores
// are already in these units.
int32_t ScoreEntry_MainScore(int32_t internal_score, uint16_t continues);

// Returns the gaiji at alphabet column 0..16, row 0..2.
uint8_t ScoreEntry_Alphabet(uint8_t col, uint8_t row);

void ScoreEntry_Init(void);

// Starts the name entry for a run's score and date; a score that does not
// qualify only waits for release and press.
void ScoreEntry_Begin(
    SCORE_SECTION *section, const SCORE_RUN *run, bool ending,
    const TH02_INPUT *input);

// Runs one frame; a completed entry requests save_section once.
SCORE_ENTRY_EVENTS ScoreEntry_Update(const TH02_INPUT *input);
