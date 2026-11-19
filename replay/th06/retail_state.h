#pragma once

#include "harness.h"

#include <stdint.h>

// Samples retail 1.02h's state after a calc, one value per column of
// g_Th06Harness, as Th06Native_Sample does for the native game.
void Th06Retail_Sample(void *context, uint32_t *row);
