#include "debug.h"

#include <signal.h>

void RunnerDebug_Break(void)
{
    raise(SIGTRAP);
}
