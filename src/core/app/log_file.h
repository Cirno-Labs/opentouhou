#pragma once

#include "core/base/result.h"

// Keeps the log of a game in a file, o<game id>.log in the user folder,
// with the name stem of the game's executable. The log still writes to the
// standard error stream too. Only core/app opens the file, so the tests,
// the replay runner and othpatch write none.

// Opens the log file of game game_id in user_dir, replacing the file of an
// earlier start, and makes the log copy each line to it (Log_SetFile).
// Lines logged before the file opens are not in it. Closes the file that
// is open first.
RESULT AppLogFile_Open(const char *user_dir, const char *game_id);

// Returns the path of the open log file, or nullptr when none is open.
const char *AppLogFile_GetPath(void);

// Stops the log from writing to the file and closes it.
void AppLogFile_Close(void);
