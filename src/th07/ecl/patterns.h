#pragma once

#include "th07/ecl.h"

#include <stdint.h>

// Runs pattern function func for enemy with arg, word 1 of the opcode 121
// or 122 instruction that selected it.
void Ecl_RunPattern(ENEMY *enemy, ECL_FUNC func, int32_t arg);
