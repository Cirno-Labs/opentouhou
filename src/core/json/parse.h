#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/base/types.h"
#include "core/json/types.h"

// Parses JSON5 text into a tree of values. thcrap's patch files are JSON
// written by hand, so the parser accepts JSON5 always, in addition to
// strict JSON:
// - Comments: // to the end of the line, and /* */.
// - A trailing comma after the last item of an array or object.
// - Keys without quotes, made of ASCII letters, digits, '_' and '$', not
//   starting with a digit.
// - Strings in single quotes. In both kinds of string, \' and \" are valid
//   escapes, and a backslash at the end of a line continues the string on
//   the next line without adding a line break. A line break in a string
//   without a backslash before it is a failure, as in JSON5.
// - Hexadecimal integers (0x1F), a leading '+', a decimal point with no
//   digits before or after it (.5, 5.), and Infinity and NaN with an
//   optional sign.
//
// The text must be UTF-8. A UTF-8 byte order mark at its start is skipped.

// Sets how deeply arrays and objects may nest. Deeper text is a failure,
// which keeps the recursive parser within its stack.
#define JSON_MAX_DEPTH 512

// Parses size bytes of JSON5 text. The text needs no NUL terminator. On
// success, *out is the root value, which may be of any type.
//
// The whole tree is one block from allocator, so it is released all at once,
// and there is no function to free it. The allocator is meant to be an
// arena. On failure, *out is nullptr and nothing is allocated from
// allocator. The parser takes its working memory from the heap and releases
// it before it returns.
//
// Bad text is never fatal. The failure message gives the position of the
// error, such as "line 3, column 17: expected ',' or '}'". Lines and columns
// count from 1, and a column counts UTF-8 characters, not bytes. Invalid
// UTF-8 in a string is a failure.
RESULT Json_Parse(
    const char *text, size_t size, const ALLOCATOR *allocator,
    JSON_VALUE **out);
