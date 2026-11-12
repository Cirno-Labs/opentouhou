#pragma once

#include "core/base/result.h"

// Names the games as they are published, and asks an installed game, or a
// new othpatch, for its version.

// Sets the size of a version, NUL included.
#define GAME_VERSION_SIZE 64

// Sets the size of a product name, NUL included.
#define GAME_NAME_SIZE 32

// Writes the product name of game, the game id with an "o" before it, into
// name: "oth06" for "th06". The executable and the packages carry this
// name, so that the executable does not clash with the original game's
// executable in the same folder. The thcrap patches keep the game id.
void Game_GetProductName(const char *game, char name[GAME_NAME_SIZE]);

// Runs the executable at path with --version and writes the version that
// it prints into version. The program prints one line, "<id> <version>",
// such as "th06 0.1.0" or "othpatch 0.2.0". The run fails when the program
// cannot start, ends with a status other than 0, or prints no such line. Its
// standard input and standard error are closed.
RESULT Game_GetVersion(const char *path, char version[GAME_VERSION_SIZE]);
