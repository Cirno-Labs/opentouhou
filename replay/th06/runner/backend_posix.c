// SPDX-License-Identifier: GPL-3.0-only

#include "debug.h"

#include <signal.h>

void RunnerDebug_Break(void)
{
    raise(SIGTRAP);
}
