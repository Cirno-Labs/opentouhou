#pragma once

#include "th02/shots/pool.h"

// Writable view of g_Shots for the module's parts.
extern SHOTS *const p_Shots;

// Fires one body/option shot for the given page, from the player's current
// state and weapon level.
void Fire_Emit(uint8_t back);
