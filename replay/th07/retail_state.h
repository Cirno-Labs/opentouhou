#pragma once

#include "harness.h"

#include <stdint.h>

// Samples retail 1.00b's state after a calc, one value per column of
// g_Th07Harness, as Th07Native_Sample does for the native game.
void Th07Retail_Sample(void *context, uint32_t *row);
