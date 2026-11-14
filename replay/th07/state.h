#pragma once

#include "fields.h"

#include <stdint.h>

// Fills row from retail memory, one value per TH07_START_FIELDS column.
void State_Start(uint32_t row[TH07_START_COUNT]);

// Fills row from retail memory, one value per TH07_CALC_FIELDS column.
void State_Calc(uint32_t row[TH07_CALC_COUNT]);
