#pragma once

#include "core/io/reader.h"

#define REIIDEN_IMAGE_SIZE 238612

// Checks REIIDEN.EXE by its length, SHA-256 and MZ header, reads it into
// scratch of REIIDEN_IMAGE_SIZE bytes and opens image over scratch.
RESULT ReiidenImage_Open(
    READER *reader, uint8_t *scratch, size_t scratch_size, READER *image);
