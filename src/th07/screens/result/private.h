#pragma once

#include "th07/screens/result/saves.h"
#include "th07/screens/result/score.h"

#include <stddef.h>
#include <stdint.h>

// Returns score.dat as the simulation last saved it, or as it was read.
const SCORE_FILE *ResultSaves_GetScoreFile(void);

// Replaces score.dat with the size encrypted bytes of data, and takes
// data, a block from g_HeapAllocator.
void ResultSaves_PutScoreFile(uint8_t *data, size_t size);

// Returns numbered replay slot slot, from 0.
const SAVED_REPLAY *ResultSaves_GetSlot(int32_t slot);

const RESULT_SCREEN_CLOCK *ResultSaves_GetClock(void);

// Fills lists with the defaults, then inserts the high scores of score.dat.
void ResultRecords_LoadLists(
    SCORE_LIST lists[SCORE_DIFFICULTY_COUNT][SCORE_SHOT_COUNT]);

// Sets name to a blank last-name record, then copies the one of score.dat
// over it. Returns whether score.dat has one.
bool ResultRecords_LoadLastName(LAST_NAME_RECORD *name);

// Saves score.dat from lists, last_name and the records of g_GameManager.
// Draws the file's key and random byte from the gameplay RNG.
void ResultRecords_WriteScore(
    const SCORE_LIST lists[SCORE_DIFFICULTY_COUNT][SCORE_SHOT_COUNT],
    const LAST_NAME_RECORD *last_name);
