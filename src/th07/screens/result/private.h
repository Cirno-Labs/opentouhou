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
