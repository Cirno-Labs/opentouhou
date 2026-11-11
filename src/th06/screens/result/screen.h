#pragma once

#include "core/base/result.h"

#include <stdint.h>

// Runs the result screen: the score browser from the title, and after a
// game the name entry, the statistics and the replay save menu. Loads the
// high score lists from score.dat when it starts, and saves score.dat when
// it ends, with two draws from the gameplay RNG.

// Adds the screen at calc priority 13 and draw priority 12. Opens the
// score browser when mode is 0. Otherwise opens the name entry after a
// game, or after a practice game records a better practice score and
// requests the title at once.
RESULT ResultScreen_Register(int32_t mode);
