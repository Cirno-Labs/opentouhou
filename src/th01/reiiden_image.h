#pragma once

#include "core/io/reader.h"

#define REIIDEN_IMAGE_SIZE 238612

// Checks the supported REIIDEN.EXE length, full SHA-256 and MZ header.
// Reads into caller-owned scratch of REIIDEN_IMAGE_SIZE bytes and opens
// image over it. Keep scratch alive while using image. Keeps reader open,
// allocates nothing and permits callers to release scratch after parsing.
RESULT ReiidenImage_Open(
    READER *reader, uint8_t *scratch, size_t scratch_size, READER *image);
