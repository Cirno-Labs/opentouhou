#pragma once

// The folders follow the rule in src/shell/folders.h.

// Returns the folder with the original game's data: the --data argument,
// or the program folder without one.
const char *Shell_GetDataDir(void);

// Returns the writable folder for settings, scores, replays and screenshots:
// the --user-dir argument, the program folder's userdata/ in portable mode,
// or the platform's user folder. Returns nullptr when the folder could not
// be found or created.
const char *Shell_GetUserDir(void);
