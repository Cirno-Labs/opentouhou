#pragma once

// The folders follow the rule in src/core/app/folders.h.

// Returns the folder with the original game's data: the --data argument,
// or the program folder without one.
const char *App_GetDataDir(void);

// Returns the writable folder for settings, scores, replays and screenshots:
// the --user-dir argument, the program folder's userdata/ in portable mode,
// or the platform's user folder. Returns nullptr when the folder could not
// be found or created.
const char *App_GetUserDir(void);
