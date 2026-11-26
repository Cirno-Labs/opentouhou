#pragma once

// Stands in for shell/game where a game runs without it: null/files.c
// gives the game its files and no patches, and null/devices.c gives no
// input, no bindings and the windowed mode, and ignores every request.
// A program that links both adds Shell_GetTickRate and Shell_GetWallClock;
// the game tests' fake links only null/files.c.

#include "shell/null/devices.h"
#include "shell/null/files.h"
