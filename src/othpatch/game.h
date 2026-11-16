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

// Sets the upper bound on the games that a GAME_LIST holds.
#define GAME_MAX_COUNT 32

// Holds the games that othpatch finds, sorted by id with no duplicate.
typedef struct {
    const char *items[GAME_MAX_COUNT];
    int32_t count;
} GAME_LIST;

// Writes the product name of game, "oth06" for "th06", into name.
void Game_GetProductName(const char *game, char name[GAME_NAME_SIZE]);

// Runs the executable at path with --version and writes the last word of
// the line that it prints, such as "0.1.0" of "th06 0.1.0", into version.
RESULT Game_GetVersion(const char *path, char version[GAME_VERSION_SIZE]);

// Adds game to list unless it is there, with a warning on out past
// GAME_MAX_COUNT games.
void Game_AddFound(GAME_LIST *list, const char *game, FILE *out);

// Adds "th<digits>" for every "oth<digits>" in exe_dir, bare or with
// ".exe", ".AppImage" or ".app", and nothing for a nullptr exe_dir.
void Game_FindBeside(
    GAME_LIST *list, ARENA *arena, const char *exe_dir, FILE *out);

// Adds every folder named "th<digits>" directly inside dir.
void Game_FindDownloaded(
    GAME_LIST *list, ARENA *arena, const char *dir, FILE *out);
