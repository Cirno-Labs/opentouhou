#pragma once

#include "core/json/types.h"

#include <stddef.h>
#include <stdint.h>

// Applies a jdiff to an ending script (data/end*.end), as thcrap's .end
// patcher applies it: a key "<n>", the 0-based number of the line that
// starts a block of text lines, gives the lines that replace the whole
// block. A line that starts with '@' is a command and counts as one line;
// any other line counts until its '\n' or the end of data. The lines keep
// their markup and translator notes.
//
// Returns a copy of the size bytes of data with diff applied, and sets
// *out_size to its size. Mem_Free releases the copy.
uint8_t *EndDiff_Apply(
    const uint8_t *data, size_t size, const JSON_VALUE *diff, size_t *out_size);
