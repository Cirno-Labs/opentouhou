#pragma once

#include "th07/replay/types.h"

#include "core/base/result.h"

#include <stddef.h>
#include <stdint.h>

// Keeps score.dat and the replays of the replay directory in memory, so
// that the simulation reads and saves them without the file system: a
// tick never opens a file. The whole replay files go to th07/replay's
// store, which playback reads. Every file is read and written through
// Shell_GetUserFiles, in the original format, after the tick that saved
// it.
//
// The replay menu asks for a scan of the replay directory when it opens,
// as the original reads the directory then. th07/main.c runs the scan
// after that tick, so the menu sees the new list from the next tick on.

// Holds the values that the result screen takes from outside the
// simulation.
typedef struct {
    char date[6]; // the local date, as MM/DD
    // Count the frames that ran and the frames that were due during play,
    // as ResultScreen_CountFrames adds them. The slowdown that the result
    // screen shows and stores comes from them.
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
    // Holds the path of the file in Shell_GetUserFiles, as
    // g_GameManager.mode.replay_path takes it.
    char path[SAVED_REPLAY_PATH_SIZE];
    char label[8]; // "No.01" to "No.15", or "User "
    REPLAY_SUMMARY summary;
} SAVED_REPLAY;

// Reads score.dat and replay/th7_01.rpy to replay/th7_15.rpy. A missing or
// invalid file counts as empty. Also reads the first 45 files of replay/
// whose names match th7_ud????.rpy, in byte order, as the original reads
// the first 45 that Windows lists. A replay's path is its path in
// Shell_GetUserFiles, spelled as listed.
void ResultScreen_OpenSaves(void);

// Writes the files that the simulation saved since the last call. Reports
// the first failure. A file that fails to write is dropped, so a later
// call never writes it.
RESULT ResultScreen_WriteSaves(void);

// Releases the files in memory and drops the saves not written.
void ResultScreen_CloseSaves(void);

// Replaces the replay at path, as g_GameManager.mode.replay_path names it,
// with the size bytes of data, and takes data, a block from
// g_HeapAllocator. Playback and the replay menu read the new bytes at
// once, also in a numbered slot that was empty. Only a path in replay/ is
// written to Shell_GetUserFiles.
void ResultScreen_PutReplayFile(const char *path, uint8_t *data, size_t size);

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

// Returns the path of numbered replay slot slot, from 0, as
// g_GameManager.mode.replay_path takes it. Mem_Free releases it.
char *ResultScreen_GetSlotPath(int32_t slot);

// Sets the date that the next ticks read, as MM/DD.
void ResultScreen_SetDate(const char *date);

// Adds one measurement of the tick rate, in ticks per second, to the frame
// counts while g_GameManager.mode.playing is set, as the original's frame
// rate display does every half second. The frames due grow by 60. The
// frames that ran grow by 60 above 54 ticks per second, by 48 above 42, by
// 36 above 30 and by 30 below.
void ResultScreen_CountFrames(float tick_rate);

// Sets the frame counts to 0, as a new game starts.
void ResultScreen_ResetFrames(void);

// Makes the replay save menu refuse every game, as it refuses a continued
// game, or accept games again. th07 refuses them while patches
// change gameplay, because such a replay would not play back in the
// original.
void ResultScreen_RefuseReplays(bool refuse);
