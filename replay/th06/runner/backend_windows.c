// SPDX-License-Identifier: GPL-3.0-only

#include "debug.h"

#include <intrin.h>

void RunnerDebug_Break(void)
{
    __debugbreak();
}
