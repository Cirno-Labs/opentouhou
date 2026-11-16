#pragma once

#include "core/base/result.h"

#include <stdint.h>

// Runs the result screen: the score browser from the title with the high
// scores, the spell cards and the play statistics, and after a game the
// name entry, the statistics and the replay save menu. Loads the high
// score lists and the last name from score.dat when it starts, and saves
// score.dat when it ends, with two draws from the gameplay RNG.

// Adds the screen at calc priority 14 and draw priority 13. Opens the
// score browser when mode is 0. Otherwise opens the name entry after a
// game, or after a practice game records a better practice score and
// opens the replay save menu.
RESULT ResultScreen_Register(int32_t mode);
