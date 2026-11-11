#pragma once

#include "core/base/result.h"

// Keeps score.dat and the headers of the replay slots in memory, so that
// the simulation reads and saves them without the file system. The files
// go to the user directory in the original format. Without a user
// directory, saves stay in memory.

// Holds the values that the result screen takes from outside the
// simulation.
typedef struct {
    char date[9]; // the local date, as MM/DD/YY
    // Count the frames that ran and the frames that were due during play.
    // The slowdown that the result screen shows and replays store comes
    // from them.
    float achieved_frames;
    float expected_frames;
} RESULT_SCREEN_CLOCK;

// Reads score.dat and replay/th6_01.rpy to replay/th6_15.rpy from
// user_dir. A missing or invalid file counts as empty. A nullptr user_dir
// reads nothing.
void ResultScreen_OpenSaves(const char *user_dir);
// Writes the files that the simulation saved since the last call. Reports
// the first failure. A file that fails to write is dropped, so a later
// call never writes it.
RESULT ResultScreen_WriteSaves(void);
// Releases the files in memory and drops the saves not written.
void ResultScreen_CloseSaves(void);
// Sets the values that the next ticks read.
void ResultScreen_SetClock(const RESULT_SCREEN_CLOCK *clock);
