// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "th06/replay/types.h"

#include "core/base/result.h"
#include "core/io/writer.h"

// Holds the save metadata in the form the file stores it.
typedef struct {
    uint8_t name[9]; // CP932, zero-padded
    char date[9];
    float slowdown; // percent, from 0 to 100
} REPLAY_FORMAT_SAVE;

// Converts info to the stored form. Reports failure when the name or date
// is missing, the date is not eight ASCII characters, or the name is longer
// than eight CP932 bytes.
RESULT ReplayFormat_Prepare(
    const REPLAY_SAVE_INFO *info, REPLAY_FORMAT_SAVE *out);
// Writes file as an original-format replay. Stores the four random bytes
// in the header, then the checksum, and encrypts the body with a key that
// starts at random[0].
RESULT ReplayFormat_Write(
    WRITER *writer, const REPLAY_FILE *file, const REPLAY_FORMAT_SAVE *info,
    const uint8_t random[4]);
