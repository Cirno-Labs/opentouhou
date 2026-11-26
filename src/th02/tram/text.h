#pragma once

// Text fields: the CP932 strings that the game's files hold for text RAM,
// decoded into characters with the codes the writes need.

#include "core/io/reader.h"

// One character, with its codes for string, truncated and single draws.
typedef struct {
    uint16_t code;
    uint16_t partial_code;
    uint16_t source_end;
    uint16_t single_code;
    uint32_t text_offset;
    bool wide;
} TRAM_CHARACTER;

// Decoded text, owned by its loading allocator.
typedef struct {
    const char *text;
    const TRAM_CHARACTER *characters;
    int32_t count;
    uint16_t source_size;
} TRAM_TEXT;

// Reads a CP932 field of up to 65535 bytes, all of it even past zero,
// keeping noncanonical selectors.
RESULT TramText_Read(
    READER *reader, size_t byte_size, const ALLOCATOR *allocator,
    TRAM_TEXT *out);
