#pragma once

// Parses the JSON texts of the core/json tests.

#include "core/base/array.h"
#include "core/base/utils.h"
#include "core/json.h"

// Returns the tree of text, parsed with allocator. A failure to parse fails
// the test case and returns nullptr.
#define TestJson_Parse(allocator_, text_)                                      \
    TestJson_ParseAt(__FILE__, __LINE__, (allocator_), (text_))

// Holds a JSON text that a test builds piece by piece, with a NUL after it.
// Array_Free releases it.
typedef ARRAY(char) TEST_JSON_TEXT;

JSON_VALUE *TestJson_ParseAt(
    const char *file, int line, const ALLOCATOR *allocator, const char *text);

// Appends value's compact JSON to buffer, with strings and keys unescaped,
// stopping at size and still counting into *used, as snprintf does.
void TestJson_DumpValue(
    const JSON_VALUE *value, char *buffer, size_t size, size_t *used);

// Appends printf-style formatted text.
PRINTF_FORMAT(2, 3)
void TestJson_Append(TEST_JSON_TEXT *text, const char *fmt, ...);

// Returns value written as compact JSON, with strings and keys unescaped,
// or "(null)" for nullptr. The string is valid until the next call.
const char *TestJson_Dump(const JSON_VALUE *value);
