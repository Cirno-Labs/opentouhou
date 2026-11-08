#pragma once

#include <windows.h>

// Makes module's timeGetTime count from 0. th06's frame wait computes in
// single precision and never ends after a few days of uptime. Returns
// whether module imports timeGetTime.
bool Clock_Install(HMODULE module);
