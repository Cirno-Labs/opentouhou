#pragma once

#include <stddef.h>

// Gives a game no input, no bindings and the windowed mode, and ignores
// every request, standing in for shell/game where a game runs without it.

// Sets how many bytes Shell_GetDeviceInput clears.
void NullShell_SetInputSize(size_t size);
