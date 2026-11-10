#pragma once

#include "../fields.h"

#include <stdint.h>

// Samples the post-first-calc checkpoint in capture column order.
void RunnerState_Start(uint32_t row[TH06_START_COUNT]);
// Samples simulation state in capture column order.
void RunnerState_Calc(uint32_t row[TH06_CALC_COUNT]);
