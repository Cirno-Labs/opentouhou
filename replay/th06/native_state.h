#pragma once

#include "harness.h"

#include <stdint.h>

// Samples the native game's state after a calc, one value per column of
// g_Th06Harness. A HASH column folds the live slots of one kind as
// fields.h gives them.
void Th06Native_Sample(void *context, uint32_t *row);
