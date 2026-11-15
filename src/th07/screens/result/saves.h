#pragma once

#include "th07/replay/types.h"

#include <stdint.h>

// Keeps the replays of the replay directory in memory, so that a tick
// never opens a file. The whole replay files go to th07/replay's store,
// which playback reads.
//
// The replay menu asks for a scan of the replay directory when it opens,
// as the original reads the directory then. th07/main.c runs the scan
// after that tick, so the menu sees the new list from the next tick on.

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
    REPLAY_SUMMARY summary;
} SAVED_REPLAY;

// Reads replay/th7_01.rpy to replay/th7_15.rpy from user_dir, then the
// first 45 files of replay/ whose names match th7_ud????.rpy, in byte
// order, as the original reads the first 45 that Windows lists. A missing
// or invalid file counts as empty. A nullptr user_dir reads nothing.
void ResultScreen_OpenSaves(const char *user_dir);

// Releases the files in memory.
void ResultScreen_CloseSaves(void);

// Asks for a scan of the replay directory after the tick.
void ResultScreen_RequestReplayScan(void);

// Scans the replay directory again when a tick asked for it: forgets the
// replays read before and reads them as ResultScreen_OpenSaves does.
void ResultScreen_ScanReplays(void);

// Returns a number that changes with each scan of the replay directory.
int32_t ResultScreen_GetReplayScan(void);

// Copies the valid replays into out in the replay menu's order: the
// numbered slots, then the th7_ud files. Returns their number.
int32_t ResultScreen_ListReplays(SAVED_REPLAY out[SAVED_REPLAY_COUNT]);
