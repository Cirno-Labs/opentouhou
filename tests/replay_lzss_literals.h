#pragma once

#include <stddef.h>
#include <stdint.h>

// Packs size bytes of data into out as LZSS literals: a 1 bit then the
// byte, most significant bit first, followed by the end marker's bits but
// not its last partial byte, as the original encoder writes. out must be
// zeroed and hold enough bytes. Returns the number of bytes written.
size_t ReplayTestLzss_EncodeLiterals(
    const uint8_t *data, size_t size, uint8_t *out);
