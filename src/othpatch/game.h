#pragma once

#include "core/base/result.h"

// Asks an installed game, or a new othpatch, for its version.

// Sets the size of a version, NUL included.
#define GAME_VERSION_SIZE 64

// Runs the executable at path with --version and writes the version that
// it prints into version. The program prints one line, "<id> <version>",
// such as "th06 0.1.0" or "othpatch 0.2.0". The run fails when the program
// cannot start, ends with a status other than 0, or prints no such line. Its
// standard input and standard error are closed.
RESULT Game_GetVersion(const char *path, char version[GAME_VERSION_SIZE]);
