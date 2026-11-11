#pragma once

// Returns the directory with the original game's data: the --data
// argument, or the executable's directory without one.
const char *App_GetDataDir(void);

// Returns the writable directory for settings, scores, replays and screenshots.
// Returns nullptr when the platform could not provide a user directory.
const char *App_GetUserDir(void);
