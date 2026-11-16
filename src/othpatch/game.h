#pragma once

#include "core/base/arena.h"
#include "core/base/result.h"
#include "core/base/types.h"

#include <stdio.h>

// Names the games as they are published, asks an installed game, or a new
// othpatch, for its version, and finds the games that othpatch handles.

// Sets the size of a version, NUL included.
#define GAME_VERSION_SIZE 64

// Sets the size of a product name, NUL included.
#define GAME_NAME_SIZE 32

// Sets the upper bound on the games that othpatch finds. More are a
// warning on the games[] array of a find function, and are ignored.
#define APP_GAME_COUNT 32

// Writes the product name of game, the game id with an "o" before it, into
// name: "oth06" for "th06". The executable, the packages and the releases
// in the release index carry this name, so that the executable does not
// clash with the original game's executable in the same folder. The thcrap
// patches keep the game id.
void Game_GetProductName(const char *game, char name[GAME_NAME_SIZE]);

// Runs the executable at path with --version and writes the version that
// it prints into version. The program prints one line, "<id> <version>",
// such as "th06 0.1.0" or "othpatch 0.2.0". The run fails when the program
// cannot start, ends with a status other than 0, or prints no such line. Its
// standard input and standard error are closed.
RESULT Game_GetVersion(const char *path, char version[GAME_VERSION_SIZE]);

// Adds game to games, which *count of them fill in sorted, byte order, with
// no duplicate. Does nothing when game is already there. Past
// APP_GAME_COUNT entries, it prints a warning on out and ignores game.
void Game_AddFound(
    const char *games[APP_GAME_COUNT], int32_t *count, const char *game,
    FILE *out);

// Adds the games beside the executable in exe_dir: every "oth<id>",
// "oth<id>.exe", "oth<id>.AppImage" or "oth<id>.app" other than othpatch
// itself, as game "th<id>". Does nothing when exe_dir is nullptr. The
// game strings come from arena.
void Game_FindBeside(
    const char *games[APP_GAME_COUNT], int32_t *count, ARENA *arena,
    const char *exe_dir, FILE *out);

// Adds the games of the folders named "th" and digits directly inside dir,
// such as a patch's directory. The game strings come from arena.
void Game_FindDownloaded(
    const char *games[APP_GAME_COUNT], int32_t *count, ARENA *arena,
    const char *dir, FILE *out);
