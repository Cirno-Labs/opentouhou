#pragma once

#include "th06/screens/result/saves.h"
#include "th06/screens/result/score.h"

#include <stddef.h>
#include <stdint.h>

#define RESULT_SAVES_SLOT_COUNT 15

// Holds the header of a replay slot that the result screen shows.
typedef struct {
    bool valid;
    uint8_t name[9]; // CP932, as the ASCII font draws it
    char date[9];
    uint8_t shot;
    uint32_t score;
} RESULT_SAVES_SLOT;

// Returns score.dat as the simulation last saved it, or as it was read.
const SCORE_FILE *ResultSaves_GetScoreFile(void);
// Replaces score.dat with the size encrypted bytes of data, and takes
// data, a block from g_HeapAllocator.
void ResultSaves_PutScoreFile(uint8_t *data, size_t size);
const RESULT_SAVES_SLOT *ResultSaves_GetSlot(int32_t slot);
// Replaces replay slot slot, from 0, with the size bytes of data, and
// takes data, a block from g_HeapAllocator.
void ResultSaves_PutReplay(int32_t slot, uint8_t *data, size_t size);
const RESULT_SCREEN_CLOCK *ResultSaves_GetClock(void);
