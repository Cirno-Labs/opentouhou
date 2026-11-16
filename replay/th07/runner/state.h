#pragma once

#include "../fields.h"

#include <stdint.h>

// Fills row from the port's state, one value per TH07_START_FIELDS column.
void RunnerState_Start(uint32_t row[TH07_START_COUNT]);

// Fills row from the port's state, one value per TH07_CALC_FIELDS column.
void RunnerState_Calc(uint32_t row[TH07_CALC_COUNT]);
