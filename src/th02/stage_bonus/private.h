#pragma once

// logic.c's own state, exposed for tests; nothing outside the
// stage_bonus/ module sees this.

#include "th02/stage_bonus/logic.h"
#include "th02/wait.h"

typedef struct {
    WAIT wait;
    bool complete;
    uint8_t waits;
} BONUS;

// A writable view of logic.c's private state, for tests.
extern BONUS *const p_Bonus;
