#pragma once

#include "th06/replay/types.h"

#include "core/io/reader.h"

#include <stddef.h>
#include <stdint.h>

// Reads, decrypts and validates an original-format replay. Allocates *out
// on success; leaves it nullptr on failure. Keeps terminal entries after
// the first end marker because stage finalization can run twice.
RESULT Replay_Read(READER *reader, REPLAY_FILE **out);
// Releases a file returned by Replay_Read. Accepts nullptr.
void Replay_Free(REPLAY_FILE *file);
// Fills out from file.
void Replay_Summarize(const REPLAY_FILE *file, REPLAY_SUMMARY *out);

// Keeps the replay files that playback reads, by the path that
// g_GameManager.mode.replay_path names, so that a tick never opens a file.
// The program adds them outside the ticks; the result screen also adds the
// replay it saves.

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
