#pragma once

#include "core/base/result.h"
#include "core/base/types.h"

// Converts text between UTF-8, which all text inside the program uses, and
// CP932, the Shift-JIS variant of Windows code page 932, which the original
// games' files use. CP932 covers ASCII, half-width katakana, JIS X 0208 and
// the NEC and IBM extensions.
//
// Both conversions are strict: they report failure for any input that does
// not convert exactly, and for a NUL byte. On failure, the output is
// nullptr. On success, it is a new NUL-terminated string that Mem_Free
// releases.

// Converts size bytes of CP932 text to UTF-8.
RESULT Cp932_Decode(const char *bytes, size_t size, char **utf8);

// Converts size bytes of UTF-8 text to CP932. A character that CP932 cannot
// represent is a failure.
RESULT Cp932_Encode(const char *utf8, size_t size, char **bytes);
