// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "th06/screens/result/saves.h"
#include "th06/screens/result/score.h"

#include <stddef.h>
#include <stdint.h>

// Returns score.dat as the simulation last saved it, or as it was read.
const SCORE_FILE *ResultSaves_GetScoreFile(void);
// Replaces score.dat with the size encrypted bytes of data, and takes
// data, a block from g_HeapAllocator.
void ResultSaves_PutScoreFile(uint8_t *data, size_t size);
const SAVED_REPLAY *ResultSaves_GetSlot(int32_t slot);
// Replaces replay slot slot, from 0, with the size bytes of data, and
// takes data, a block from g_HeapAllocator.
void ResultSaves_PutReplay(int32_t slot, uint8_t *data, size_t size);
const RESULT_SCREEN_CLOCK *ResultSaves_GetClock(void);
// Returns whether ResultScreen_RefuseReplays refuses replays.
bool ResultSaves_RefusesReplays(void);
