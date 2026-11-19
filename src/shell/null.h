#pragma once

#include "core/base/result.h"

#include <stddef.h>

// Stands in for shell/game where a game runs without it: null/files.c
// gives the game its files and no patches, and null/devices.c gives no
// input, no bindings and the windowed mode, and ignores every request.
// A program that links both adds Shell_GetTickRate and Shell_GetWallClock;
// the game tests' fake links only null/files.c.

// Makes Shell_GetFiles hold data_dir, read-only, and Shell_GetUserFiles
// hold user_dir, writable. Each holds nothing for a nullptr directory, as
// it does before the first call.
RESULT NullShell_OpenFiles(const char *data_dir, const char *user_dir);

// Destroys what NullShell_OpenFiles made.
void NullShell_CloseFiles(void);

// Sets how many bytes Shell_GetDeviceInput clears.
void NullShell_SetInputSize(size_t size);
