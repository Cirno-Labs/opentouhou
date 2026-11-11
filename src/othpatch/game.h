#pragma once

#include "core/base/result.h"

// Asks an installed game for its version.

// Sets the size of a version, NUL included.
#define GAME_VERSION_SIZE 64

// Runs the game executable at path with --version and writes the version
// that it prints into version. The game prints one line, "<game id>
// <version>", such as "th06 0.1.0". The run fails when the game cannot
// start, ends with a status other than 0, or prints no such line. Its
// standard input and standard error are closed.
RESULT Game_GetVersion(const char *path, char version[GAME_VERSION_SIZE]);
