#pragma once

#include "core/vfs/types.h"

// Sets up the virtual filesystems a game reads its data from and writes
// its saves to. The folders follow the rule in src/shell/folders.h.

// Returns the game's data files: the data folder, which is the --data
// argument or the program folder, read-only, at priority 0. The game
// mounts its archives and patches above it. Valid from before init to
// after shutdown.
VFS *Shell_GetFiles(void);

// Returns the files that the game keeps, such as scores and replays: the
// user folder, which is the --user-dir argument, the program folder's
// userdata/ in portable mode or the platform's user folder, writable, at
// priority 0, or an empty VFS that refuses writes when there is no user
// folder. Valid from before init to after shutdown.
VFS *Shell_GetUserFiles(void);
