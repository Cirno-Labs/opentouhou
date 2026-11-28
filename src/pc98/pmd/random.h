#pragma once

#include "core/base/types.h"

// Advances KAJA PMD's shared 15-bit generator and scales its state by
// limit / 32767. Returns 0..limit inclusive. Accepts any initial seed word;
// stores a 15-bit seed after the first call. A zero limit still advances.
// Derives behavior from https://github.com/takamichih/pmdsrc and the
// original driver. Keep one seed per driver, shared across its LFOs.

uint16_t PmdRandom_Next(uint16_t *seed, uint16_t limit);
