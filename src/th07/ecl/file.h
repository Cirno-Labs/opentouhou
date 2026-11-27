#pragma once

#include "th07/ecl/types.h"

#include "core/base/result.h"

#include <stdint.h>

// Loads an enemy script file (ECL) and holds it as module state, so that
// Ecl_RunEnemy and the timelines of enemy.h can read it.

// Loads the enemy script file of g_GameManager.run.stage. A file whose
// header, subroutine table, timelines or instructions run past its end,
// whose instruction is shorter than its opcode reads, that jumps to a
// position that is not an instruction of its subroutine, that names a
// subroutine, pattern function or curve it does not have, or whose spell
// card has no CP932 name in its 48 bytes or a number past the last card,
// fails the load.
RESULT Ecl_Load(void);

// Releases the file loaded by Ecl_Load.
void Ecl_Release(void);

// Returns the number of timelines of the loaded file.
int32_t Ecl_GetTimelineCount(void);

// Returns timeline index of the loaded file, ended by a negative time.
const ECL_TIMELINE_ENTRY *Ecl_GetTimeline(int32_t index);
