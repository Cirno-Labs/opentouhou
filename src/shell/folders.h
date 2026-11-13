#pragma once

#include "core/base/result.h"

// Decides where a game finds its data and keeps its files.
//
// The program folder is the folder that the player put the game into:
// - on Linux, inside an AppImage, the folder of the .AppImage file. The
//   AppImage runtime names the file in $APPIMAGE and its mount in
//   $APPDIR. $APPIMAGE counts only while the executable is inside $APPDIR,
//   so a variable that another AppImage left behind has no effect;
// - on macOS, inside an application bundle, the folder that holds the
//   bundle, whether the executable reports its Resources or its MacOS
//   folder;
// - elsewhere, the folder of the executable.
//
// The data folder is the --data argument, or the program folder without
// one. A file portable.txt in the program folder turns on portable mode.
// The user folder and the patches folder then follow this rule:
//
// | Mode                       | User folder               | Patches folder |
// | -------------------------- | ------------------------- | -------------- |
// | default                    | the platform's, per game  | the platform's |
// | portable                   | <program folder>/userdata | <program>/patches
// | | --user-dir DIR             | DIR                       | the platform's |
// | --user-dir DIR, portable   | DIR                       | <program>/patches
// |
//
// The platform's folders are those of Path_GetUserDir and
// Path_GetPatchesDir. --user-dir moves only the user folder of one game.
// The patches folder depends only on portable mode, so that every game of
// one installation shares it. Portable mode never writes into the program
// folder itself, so the original game's own files there stay untouched.

// Names the file that turns on portable mode.
#define FOLDERS_PORTABLE_FILE "portable.txt"

typedef enum {
    FOLDERS_DEFAULT,
    FOLDERS_PORTABLE,
    FOLDERS_USER_DIR,
    FOLDERS_USER_DIR_PORTABLE,
} FOLDERS_MODE;

// Holds what the folders come from. Each member is nullptr when it is not
// given.
typedef struct {
    // Holds the folder of the executable, as Path_GetExeDir gives it.
    const char *exe_dir;
    // Holds $APPIMAGE and $APPDIR.
    const char *appimage;
    const char *appdir;
    // Holds argv[0].
    const char *program;
    // Holds the --data and the --user-dir arguments.
    const char *data_dir;
    const char *user_dir;
} FOLDERS_INPUT;

// Holds the folders. Each string is nullptr when it could not be found.
typedef struct {
    FOLDERS_MODE mode;
    char *program_dir;
    // Holds the file name of the program, without its folder: the
    // .AppImage file's inside an AppImage, argv[0]'s elsewhere.
    char *program_name;
    char *data_dir;
    char *user_dir;
} FOLDERS;

// Finds the folders of game game_id and creates the user folder. Reports
// failure when the user folder cannot be found or created; the other
// folders are set then too.
RESULT Folders_Find(
    const FOLDERS_INPUT *input, const char *game_id, FOLDERS *out);
// Returns the patches folder of the mode in folders, and creates it, as
// Path_GetPatchesDir does. Mem_Free releases *out. On failure, *out is
// nullptr.
RESULT Folders_GetPatchesDir(const FOLDERS *folders, char **out);
// Returns the patches folder that the games of input's program folder
// use, and creates it, as Folders_GetPatchesDir does. Finds no game's
// folders, so it creates no user folder. Mem_Free releases *out. On
// failure, *out is nullptr.
RESULT Folders_FindPatchesDir(const FOLDERS_INPUT *input, char **out);
// Returns the name of a mode, as --help prints it.
const char *Folders_GetModeName(FOLDERS_MODE mode);
// Releases the strings of folders.
void Folders_Free(FOLDERS *folders);
