#pragma once

#include <windows.h>

#include <stdint.h>

// Makes module's timeGetTime and QueryPerformanceCounter count from 0, on
// one clock. With step 0 they follow real time; otherwise each call moves
// the clock step ms on. QueryPerformanceFrequency reports 1,000,000 per
// second. Returns whether module imports either.
bool Clock_Install(HMODULE module, uint32_t step);
