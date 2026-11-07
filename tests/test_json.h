#pragma once

// Parses the JSON texts of the core/json tests.

#include "core/json.h"

#include "test.h"

// Returns the tree of text, parsed with allocator. A failure to parse fails
// the test case and returns nullptr.
#define TestJson_Parse(allocator_, text_)                                      \
    TestJson_ParseAt(__FILE__, __LINE__, (allocator_), (text_))

static JSON_VALUE *TestJson_ParseAt(
    const char *const file, const int line, const ALLOCATOR *const allocator,
    const char *const text)
{
    JSON_VALUE *value = nullptr;
    const RESULT result = Json_Parse(text, strlen(text), allocator, &value);
    if (!IS_OK(result)) {
        printf("    %s:%d: %s\n", file, line, result.message);
        m_TestFailures++;
    }
    Result_Ignore(result);
    return value;
}
