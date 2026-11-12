#pragma once

#include <stddef.h>

// Shows the fatal errors of a game to the player in a message box. A
// release build on Windows has no console, so the log alone reaches no
// player.
//
// Only core/app calls AppFatal_Install, and only the game programs link
// core/app, so the tests, the replay runner and othpatch never show a
// box. A game program shows none either in a headless run: when
// $SDL_VIDEODRIVER is "offscreen" or "dummy", or when $CI is set and not
// empty. A box comes after the log line of its error. It holds the thread
// of the main loop until the player closes it; the program then ends, so
// the box never changes the simulation.

// Returns whether a game program with these environment values shows
// message boxes. Each value is nullptr when its variable is not set.
bool AppFatal_UsesBox(const char *video_driver, const char *ci);

// Decides from the environment whether game game_id shows message boxes.
// When it does, sets a fatal handler (Log_SetFatalHandler) that shows each
// fatal error as an internal error.
void AppFatal_Install(const char *game_id);

// Shows text in a box that says that the game could not start. Does
// nothing when the boxes are off.
void AppFatal_ShowStartupError(const char *text);

// Writes the text for a game data file that the data folder lacks into
// out, truncated to size bytes.
void AppFatal_FormatMissingData(
    char *out, size_t size, const char *file, const char *data_dir);

// Writes the text for a fatal error that is not the player's to fix into
// out, truncated to size bytes. file and line give the source position.
void AppFatal_FormatInternalError(
    char *out, size_t size, const char *game_id, const char *message,
    const char *file, int line);
