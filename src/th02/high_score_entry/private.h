#pragma once

// logic.c's own state and menu step, exposed for tests; nothing outside
// the high_score_entry/ directory sees this.

#include "th02/high_score_entry/logic.h"
#include "th02/wait.h"

typedef enum {
    HIGH_SCORE_ENTRY_READ,
    HIGH_SCORE_ENTRY_TAIL,
    HIGH_SCORE_ENTRY_UNQUALIFIED,
    HIGH_SCORE_ENTRY_DONE,
} HIGH_SCORE_ENTRY_PHASE;

typedef struct {
    HIGH_SCORE_SECTION *section;
    uint16_t keys;
    uint16_t input_locked;
    uint8_t input_delay;
    uint8_t col;
    uint8_t row;
    uint8_t name_pos;
    int8_t place;
    HIGH_SCORE_ENTRY_PHASE phase;
} HIGH_SCORE_ENTRY_MENU;

typedef struct {
    HIGH_SCORE_ENTRY_EVENTS event;
    WAIT wait;
} HIGH_SCORE_ENTRY;

// Writable views of logic.c's private state, for tests.
extern HIGH_SCORE_ENTRY_MENU *const p_Menu;
extern HIGH_SCORE_ENTRY *const p_Entry;

// The menu step logic.c's public functions drive; exposed so tests can
// drive it directly without going through input/wait.
HIGH_SCORE_ENTRY_EVENTS HighScoreEntryMenu_Begin(
    HIGH_SCORE_SECTION *section, const SCORE_RUN *run, bool ending);
HIGH_SCORE_ENTRY_EVENTS HighScoreEntryMenu_Step(uint16_t keys);
