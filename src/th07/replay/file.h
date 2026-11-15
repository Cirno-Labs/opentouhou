#pragma once

#include "th07/replay/types.h"

#include "core/io/reader.h"

#include <stddef.h>
#include <stdint.h>

// Reads, decrypts, decompresses and validates an original-format replay.
// Allocates *out on success; leaves it nullptr on failure. Rejects a replay
// recorded with scores disabled, as the original does, but keeps the
// executable identity without checking it.
RESULT Replay_Read(READER *reader, REPLAY_FILE **out);

// Releases a file returned by Replay_Read. Accepts nullptr.
void Replay_Free(REPLAY_FILE *file);

// Fills out from file.
void Replay_Summarize(const REPLAY_FILE *file, REPLAY_SUMMARY *out);

// Keeps the replay files that playback reads, by path, so that a tick
// never opens a file. The program adds them outside the ticks.

// Keeps a copy of the size bytes of data for path, in place of an earlier
// copy for the same path.
void Replay_AddFile(const char *path, const void *data, size_t size);

// Forgets the copy for path. Does nothing when there is none.
void Replay_RemoveFile(const char *path);

// Forgets every copy.
void Replay_ClearFiles(void);

// Returns the bytes kept for path and sets *size to their number, or
// returns nullptr when none are kept.
const uint8_t *Replay_FindFile(const char *path, size_t *size);
