#pragma once

// Loads and runs the enemy scripts of a stage (ECL): the subroutines that
// move each enemy, fire its patterns, run its spell cards and branch on
// its state, and the 16 timelines that enemy.h walks to spawn enemies.
// The loaded file is module state. Timers and movement run at g_GameSpeed,
// which pattern functions 10 and 11 change. The shared variables 10037 to
// 10044 last across loads, as in the original.

#include "th07/ecl/file.h"
#include "th07/ecl/patterns.h"
#include "th07/ecl/run.h"
#include "th07/ecl/types.h"
