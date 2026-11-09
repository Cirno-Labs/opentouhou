#pragma once

// Returns the directory with the original game's data: the --data
// argument, or the executable's directory without one.
const char *App_GetDataDir(void);
