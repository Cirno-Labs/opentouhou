#pragma once

#include <stddef.h>

// Sets the size that Shell_GetDeviceInput zeroes, from the game's
// input_schema. Call once before the game's input is first read.
void RunnerShell_SetInputSize(size_t size);
