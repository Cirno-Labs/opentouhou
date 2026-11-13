// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/result.h"
#include "core/json/types.h"

#include <stddef.h>
#include <stdint.h>

// Holds an ending script of data/end*.end. A script is CP932 text. A line
// that starts with '@' is a command; its arguments are decimal numbers or
// paths, each ended by NUL bytes. Any other line is text, which shows two
// bytes at a time in halves of 32 bytes, two halves to a screen line. The
// original splits by bytes, so the script splits the CP932 bytes and
// converts each half that can start in the script to UTF-8 when it loads.
// A half that does not convert is empty.
//
// A script that a patch replaces or has a jdiff for shows each text line
// whole, as thcrap's base_tsa patch makes the original do: the line is one
// half, from its start to its NUL or line break, and text that is valid
// UTF-8 loads as UTF-8. The jdiff applies as thcrap's .end patcher applies
// it: a key "<n>", the 0-based number of the line that starts a block of
// text lines, gives the lines that replace the whole block. They keep
// their markup and translator notes, for Anm_RenderText to lay out.

#define ENDING_HALF_BYTES 0x20

typedef struct {
    uint8_t *data;
    size_t size;
    // Holds the UTF-8 text of the half that starts at each byte of data,
    // where one can start, and nullptr elsewhere.
    char **halves;
    // Tells that each text line is one half, as the patches make it.
    bool whole_lines;
} ENDING_SCRIPT;

// Loads the script at path through th06/files, with the jdiff of the
// patches. Reports failure when the file cannot be read; *out is then
// empty.
RESULT EndingScript_Load(const char *path, ENDING_SCRIPT *out);
// Prepares a script over a copy of the size bytes of data, with whole
// lines or with halves of 32 bytes.
void EndingScript_Init(
    const void *data, size_t size, bool whole_lines, ENDING_SCRIPT *out);
// Returns a copy of the size bytes of data with the jdiff diff applied,
// and sets *out_size to its size. Mem_Free releases the copy.
uint8_t *EndingScript_ApplyDiff(
    const uint8_t *data, size_t size, const JSON_VALUE *diff, size_t *out_size);
// Releases a script and leaves it empty.
void EndingScript_Free(ENDING_SCRIPT *script);
// Returns the byte at offset, or 0 past the end.
uint8_t EndingScript_Byte(const ENDING_SCRIPT *script, size_t offset);
// Returns the end of the half that starts at start: the byte after up to
// 32 bytes of pairs, or the command or line break that ends it first.
size_t EndingScript_HalfEnd(const ENDING_SCRIPT *script, size_t start);
// Returns the UTF-8 text of the half that starts at offset, or nullptr
// where no half starts. Halves start at the start of the script, after a
// line break or a NUL, and after a full half. With whole lines, a half
// starts only at the start of a text line.
const char *EndingScript_GetHalf(const ENDING_SCRIPT *script, size_t offset);
