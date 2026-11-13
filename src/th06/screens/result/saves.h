// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "th06/replay/types.h"

#include "core/base/result.h"

#include <stdint.h>

// Keeps score.dat and the replays of the replay directory in memory, so
// that the simulation reads and saves them without the file system: a
// tick never opens a file. The whole replay files go to th06/replay's
// store, which playback reads. The files go to the user directory in the
// original format, after the tick that saved them. Without a user
// directory, saves stay in memory.
//
// The replay menu asks for a scan of the replay directory when it opens,
// as the original reads the directory then. th06/main.c runs the scan
// after that tick, so the menu sees the new list from the next tick on.

// Holds the values that the result screen takes from outside the
// simulation.
typedef struct {
    char date[9]; // the local date, as MM/DD/YY
    // Count the frames that ran and the frames that were due during play,
    // as ResultScreen_CountFrames adds them. The slowdown that the result
    // screen shows and replays store comes from them.
    float achieved_frames;
    float expected_frames;
} RESULT_SCREEN_CLOCK;

#define SAVED_REPLAY_SLOT_COUNT 15
#define SAVED_REPLAY_USER_COUNT 45
#define SAVED_REPLAY_COUNT (SAVED_REPLAY_SLOT_COUNT + SAVED_REPLAY_USER_COUNT)
#define SAVED_REPLAY_PATH_SIZE 0x200

// Holds a replay of the replay directory as the menus show it.
typedef struct {
    bool valid;
    // Holds the path of the file, as g_GameManager.mode.replay_path takes
    // it.
    char path[SAVED_REPLAY_PATH_SIZE];
    char label[8]; // "No.01" to "No.15", or "User "
    uint8_t name[9]; // CP932, as the ASCII font draws it
    REPLAY_SUMMARY summary;
} SAVED_REPLAY;

// Reads score.dat and replay/th6_01.rpy to replay/th6_15.rpy from
// user_dir. A missing or invalid file counts as empty. A nullptr user_dir
// reads nothing. Also reads the first 45 files of replay/ whose names
// match th6_ud????.rpy, in byte order, as the original reads the first 45
// that Windows lists.
void ResultScreen_OpenSaves(const char *user_dir);
// Writes the files that the simulation saved since the last call. Reports
// the first failure. A file that fails to write is dropped, so a later
// call never writes it.
RESULT ResultScreen_WriteSaves(void);
// Releases the files in memory and drops the saves not written.
void ResultScreen_CloseSaves(void);
// Asks for a scan of the replay directory after the tick.
void ResultScreen_RequestReplayScan(void);
// Scans the replay directory again when a tick asked for it: forgets the
// replays read before and reads the slots and the user replays, as
// ResultScreen_OpenSaves does.
void ResultScreen_ScanReplays(void);
// Returns a number that changes with each scan of the replay directory.
int32_t ResultScreen_GetReplayScan(void);
// Copies the valid replays into out in the replay menu's order: the
// numbered slots, as the simulation last saved them, then the th6_ud files.
// Returns their number.
int32_t ResultScreen_ListReplays(SAVED_REPLAY out[SAVED_REPLAY_COUNT]);
// Sets the date that the next ticks read, as MM/DD/YY.
void ResultScreen_SetDate(const char *date);
// Adds one measurement of the tick rate, in ticks per second, to the frame
// counts while g_GameManager.mode.playing is set, as the original's frame
// rate display does every half second. The frames due grow by 60. The
// frames that ran grow by 60 above 54 ticks per second, by 48 above 42, by
// 36 above 30 and by 30 below.
void ResultScreen_CountFrames(float tick_rate);
// Sets the frame counts to 0, as a new game starts.
void ResultScreen_ResetFrames(void);
// Makes the replay save menu refuse every game, as it refuses a game with
// a continue, or accept games again. th06 refuses them while patches
// change gameplay, because such a replay would not play back in the
// original.
void ResultScreen_RefuseReplays(bool refuse);
