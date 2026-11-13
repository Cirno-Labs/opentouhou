#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"

// Builds paths and finds the directories that the games read and write.
// Paths are UTF-8. The directories that the Path_Get functions return end
// with the platform's separator: '\' on Windows, '/' elsewhere.

// Returns a and b joined by a '/', as a new string from allocator that
// Allocator_Free(allocator, path, strlen(path) + 1) releases. No '/' is
// added when a is empty or already ends with '/' or '\'.
char *Path_Join(const ALLOCATOR *allocator, const char *a, const char *b);

// Returns the directory that holds the executable, or nullptr when the
// platform cannot tell. The string stays valid for the life of the program.
const char *Path_GetExeDir(void);

// Returns the user directory of a game, where its settings, scores and
// replays go, and creates it if it does not exist. game_id is the id from
// the GAME_DESC. The directory is "opentouhou/<game_id>" in the platform's
// directory for application data: $XDG_DATA_HOME (by default
// ~/.local/share) on Linux, %APPDATA% on Windows and ~/Library/Application
// Support on macOS. Mem_Free releases *out. On failure, *out is nullptr.
RESULT Path_GetUserDir(const char *game_id, char **out);

// Returns the folder that holds the thcrap patches of every game, and
// creates it if it does not exist. Without a portable_dir, it is
// "opentouhou/patches" in the same platform directory as the user
// directories, so that it sits beside them. With a portable_dir, which is
// the program folder in portable mode, it is "patches" in portable_dir.
// Mem_Free releases *out. On failure, *out is nullptr.
RESULT Path_GetPatchesDir(const char *portable_dir, char **out);
