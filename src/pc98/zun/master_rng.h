#pragma once

#include "core/base/types.h"

// Advances the master.lib state and returns its upper 15 bits; a new
// state starts at 1.
uint16_t MasterRng_Next(uint32_t *state);
