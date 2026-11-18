#pragma once

#include "runner.h"

typedef struct SESSION_COMPARE SESSION_COMPARE;

// Opens a version-1 case for a finite scripted session. Keeps input open
// until Close; the caller owns input. Reuses the runner's named columns.
RESULT SessionCompare_Open(
    READER *input, const RUNNER_GAME *game, SESSION_COMPARE **out);

// Samples every completed calc, including chain restarts. Begins when
// ready is first true and stops when ended is true. The game shim uses
// the same boundaries as its retail recorder. Does not stop the chain.
void SessionCompare_Calc(SESSION_COMPARE *compare, bool ready, bool ended);

// Compares a sampled row without calling the game's snapshot callbacks.
// start and calc follow the schema passed to Open. Empty START tables
// allow nullptr for start.
void SessionCompare_Row(
    SESSION_COMPARE *compare, const uint32_t *start, const uint32_t *calc);

// Requires all case rows and the complete current column schema to match,
// with no extra recorded calcs. Checks this session's finite recording;
// it does not certify full replay coverage. Releases compare.
RESULT SessionCompare_Close(SESSION_COMPARE *compare, RUNNER_REPORT *report);
