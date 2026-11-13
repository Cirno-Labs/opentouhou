#pragma once

#include "types.h"

#include "../../core/case.h"
#include "../fields.h"

typedef struct RUNNER_COMPARE RUNNER_COMPARE;

// Prepares named column comparisons and reads the first expected calc.
// Requires at least one implemented calc column. Uses options.output for
// diagnostics when it is not nullptr. Keeps reader until Close.
RESULT RunnerCompare_Open(
    CASE_READER *reader, const RUNNER_OPTIONS *options, RUNNER_COMPARE **out);
// Compares one completed calc. Compares START only on the first call,
// because the capture samples it after the first gameplay calc. Returns
// whether another calc is expected; false also stops at the first diff.
bool RunnerCompare_Calc(
    RUNNER_COMPARE *compare, const uint32_t start[TH06_START_COUNT],
    const uint32_t calc[TH06_CALC_COUNT]);
// Returns whether comparison has reached EOF or its first difference.
bool RunnerCompare_IsDone(const RUNNER_COMPARE *compare);
// Closes decompression, reports corrupt streams and releases compare.
// Stores comparison totals in report. Does not free the reader's arena.
RESULT RunnerCompare_Close(RUNNER_COMPARE *compare, RUNNER_REPORT *report);
