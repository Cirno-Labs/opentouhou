#pragma once

#include "th06/replay/types.h"

#include "core/io/reader.h"

// Reads, decrypts and validates an original-format replay. Allocates *out
// on success; leaves it nullptr on failure. Keeps terminal entries after
// the first end marker because stage finalization can run twice.
RESULT Replay_Read(READER *reader, REPLAY_FILE **out);
// Releases a file returned by Replay_Read. Accepts nullptr.
void Replay_Free(REPLAY_FILE *file);
