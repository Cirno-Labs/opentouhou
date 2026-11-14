#pragma once

#include "core/base/result.h"
#include "core/base/types.h"
#include "core/io/reader.h"
#include "core/io/writer.h"

// ZUN's LZSS coder, as the PBG3 and PBG4 archives, th07's replays and
// score.dat use it: bits most significant first, a 0x2000-byte window
// that starts at position 1, a 1 bit then a literal byte, or a 0 bit,
// a 13-bit window offset and a 4-bit length (3 to 18 bytes). Offset 0
// ends the stream.

// Decodes from input into output, which holds size bytes, until size
// bytes are written or the stream ends. The window starts as zeros on every
// call. Sets *decoded to the bytes
// written. A match that runs past size is a failure. Sets *checksum,
// when not nullptr, to the wrapping sum of the bytes read from input
// (PBG3's entry check). Bits past the end of input read as 0, as the
// encoder never writes the last partial byte; they set the error flag
// of input.
RESULT Lzss_Decode(
    READER *input, uint8_t *output, size_t size, size_t *decoded,
    uint32_t *checksum);

// Encodes size bytes of data into output with th07's binary-tree match
// search, byte for byte as the original writes them. The bits of the
// last partial byte, the end of the end marker, are not written.
void Lzss_Encode(const uint8_t *data, size_t size, WRITER *output);
