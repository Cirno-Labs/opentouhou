#pragma once

#include "core/base/result.h"

#include <stddef.h>
#include <stdint.h>

// Holds an ending script of data/end*.end. A script is CP932 text. A line
// that starts with '@' is a command; its arguments are decimal numbers or
// paths, each ended by NUL bytes. Any other line is text, shown whole on
// one of the ending's fifteen rows (the original has no half-splitting:
// it accumulates a whole line, then renders it in one call).
//
// A jdiff for the script applies as thcrap's .end patcher applies it: a
// key "<n>", the 0-based number of the line that starts a block of text
// lines, gives the lines that replace the whole block. They keep their
// markup and translator notes, for Anm_RenderText to lay out.

typedef struct {
    uint8_t *data;
    size_t size;
    // Holds the UTF-8 text of the line that starts at each byte of data,
    // where a line can start, and nullptr elsewhere.
    char **lines;
} ENDING_SCRIPT;

// Loads the script at path through th07/files, with the jdiff of the
// patches. Reports failure when the file cannot be read; *out is then
// empty.
RESULT EndingScript_Load(const char *path, ENDING_SCRIPT *out);

// Prepares a script over a copy of the size bytes of data.
void EndingScript_Init(const void *data, size_t size, ENDING_SCRIPT *out);

// Releases a script and leaves it empty.
void EndingScript_Free(ENDING_SCRIPT *script);

// Returns the byte at offset, or 0 past the end.
uint8_t EndingScript_Byte(const ENDING_SCRIPT *script, size_t offset);

// Returns the UTF-8 text of the line that starts at offset, or nullptr
// where no line starts. Lines start at the start of the script and after
// a line break or a NUL.
const char *EndingScript_GetLine(const ENDING_SCRIPT *script, size_t offset);
