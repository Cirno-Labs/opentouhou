#pragma once

#include "core/base/result.h"

#include <stddef.h>
#include <stdint.h>

// Holds an ending script of data/end*.end. A script is CP932 text. A line
// that starts with '@' is a command; its arguments are decimal numbers or
// paths, each ended by NUL bytes. Any other line is text, which shows two
// bytes at a time in halves of 32 bytes, two halves to a screen line. The
// original splits by bytes, so the script splits the CP932 bytes and
// converts each half that can start in the script to UTF-8 when it loads.
// A half that does not convert is empty.

#define ENDING_HALF_BYTES 0x20

typedef struct {
    uint8_t *data;
    size_t size;
    // Holds the UTF-8 text of the half that starts at each byte of data,
    // where one can start, and nullptr elsewhere.
    char **halves;
} ENDING_SCRIPT;

// Loads the script at path through th06/files. Reports failure when the
// file cannot be read; *out is then empty.
RESULT EndingScript_Load(const char *path, ENDING_SCRIPT *out);
// Prepares a script over a copy of the size bytes of data.
void EndingScript_Init(const void *data, size_t size, ENDING_SCRIPT *out);
// Releases a script and leaves it empty.
void EndingScript_Free(ENDING_SCRIPT *script);
// Returns the byte at offset, or 0 past the end.
uint8_t EndingScript_Byte(const ENDING_SCRIPT *script, size_t offset);
// Returns the end of the half that starts at start: the byte after up to
// 32 bytes of pairs, or the command or line break that ends it first.
size_t EndingScript_HalfEnd(const ENDING_SCRIPT *script, size_t start);
// Returns the UTF-8 text of the half that starts at offset, or nullptr
// where no half starts. Halves start at the start of the script, after a
// line break or a NUL, and after a full half.
const char *EndingScript_GetHalf(const ENDING_SCRIPT *script, size_t offset);
