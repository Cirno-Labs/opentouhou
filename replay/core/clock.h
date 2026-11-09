#pragma once

#include <windows.h>

#include <stdint.h>

// Makes module's timeGetTime count from 0. With step 0 it follows real
// time; otherwise each call returns step ms more than the one before.
// th06's frame wait computes in single precision and never ends after a
// few days of uptime. Returns whether module imports timeGetTime.
bool Clock_Install(HMODULE module, uint32_t step);
