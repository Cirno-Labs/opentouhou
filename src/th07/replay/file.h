#pragma once

#include "th07/replay/types.h"

#include "core/io/reader.h"
#include "core/io/writer.h"

#include <stddef.h>
#include <stdint.h>

// Reads, decrypts, decompresses and validates an original-format replay.
// Allocates *out on success; leaves it nullptr on failure. Rejects a replay
// recorded with scores disabled, as the original does, but keeps the
// executable identity without checking it.
RESULT Replay_Read(READER *reader, REPLAY_FILE **out);

// Writes file as an original-format replay: lays out its stages and frame
// rates, compresses them with Lzss_Encode, then stores the checksum and
// encrypts the body with a key that starts at random[0].
RESULT Replay_Write(
    const REPLAY_FILE *file, WRITER *writer, const uint8_t random[3]);

// Releases a file returned by Replay_Read. Accepts nullptr.
void Replay_Free(REPLAY_FILE *file);

// Fills out from file.
void Replay_Summarize(const REPLAY_FILE *file, REPLAY_SUMMARY *out);
