#pragma once

#include "th07/ecl/types.h"

#include <stdint.h>

// Runs the subroutines of the loaded enemy script file (ecl/file.h) on an
// enemy's state: its instructions, movement, timed shots and curves.

// Starts subroutine sub on state at its frame 0. Keeps the variables, the
// frame function, the curves and the timed call mark of state.
void Ecl_StartSub(ECL_STATE *state, int16_t sub);

// Runs a frame of the script of enemy: a pending interrupt or a due
// periodic call, the instructions due unless opcode 45's wait holds them,
// the movement and, while the enemy has life, the timed shots, the sprite
// direction, the frame function and the curves; then boss 0's spell card
// bonus. Returns ECL_RUN_DELETE once the script ends.
ECL_RUN Ecl_RunEnemy(ENEMY *enemy);
