#pragma once

#include "core/base/result.h"

// Gives a game its files and no patches, standing in for shell/game where
// a game runs without it.

// Makes Shell_GetFiles hold data_dir, read-only, as ShellData_Mount mounts
// it, and Shell_GetUserFiles hold user_dir, writable. Each holds nothing for a
// nullptr directory, as it does before the first call.
RESULT NullShell_OpenFiles(const char *data_dir, const char *user_dir);

// Destroys what NullShell_OpenFiles made.
void NullShell_CloseFiles(void);
