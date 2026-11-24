#pragma once

#include "core/io/reader.h"

typedef struct DEFLATE_INDEX DEFLATE_INDEX;

// Takes ownership of packed, including on failure. Scans the raw deflate
// stream and checks its size and CRC-32 only when *index is nullptr, then
// saves seek checkpoints in *index. Reopens use those immutable checkpoints.
// Reuse an index only for the same unchanged stream. Both the index and
// the source of packed must stay valid until out closes.
RESULT Deflate_Open(
    READER *packed, size_t size, uint32_t crc, const ALLOCATOR *allocator,
    DEFLATE_INDEX **index, READER *out);

// Releases an index after every reader that uses it has closed.
void Deflate_FreeIndex(DEFLATE_INDEX *index);
