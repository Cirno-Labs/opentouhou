#pragma once

#include "core/io/reader.h"

// Takes ownership of packed, including on failure. Scans a raw deflate
// stream, checks its unpacked size and CRC-32, and builds seek checkpoints.
// On success, out owns the decoder and checkpoints. The source of packed
// must stay valid until out closes.
RESULT Deflate_Open(
    READER *packed, size_t size, uint32_t crc, const ALLOCATOR *allocator,
    READER *out);
