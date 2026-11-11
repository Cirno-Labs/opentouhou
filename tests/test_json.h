#pragma once

// Parses the JSON texts of the core/json tests.

#include "core/base/array.h"
#include "core/base/string.h"
#include "core/json.h"

#include "test.h"

#include <stdarg.h>

// Returns the tree of text, parsed with allocator. A failure to parse fails
// the test case and returns nullptr.
#define TestJson_Parse(allocator_, text_)                                      \
    TestJson_ParseAt(__FILE__, __LINE__, (allocator_), (text_))

[[maybe_unused]] static JSON_VALUE *TestJson_ParseAt(
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

[[maybe_unused]] static void M_TestJsonDump(
    const JSON_VALUE *const value, char *const buffer, const size_t size,
    size_t *const used)
{
#define M_PRINT(...)                                                           \
    *used += (size_t)snprintf(                                                 \
        buffer + MIN(*used, size), size - MIN(*used, size), __VA_ARGS__)
    switch (value->type) {
    case JSON_NULL:
        M_PRINT("null");
        break;
    case JSON_BOOL:
        M_PRINT("%s", value->boolean ? "true" : "false");
        break;
    case JSON_NUMBER:
        if (value->number.is_integer) {
            M_PRINT("%lld", (long long)value->number.integer);
        } else {
            M_PRINT("%g", value->number.value);
        }
        break;
    case JSON_STRING:
        M_PRINT("\"%s\"", value->string.data);
        break;
    case JSON_ARRAY:
        M_PRINT("[");
        for (int32_t i = 0; i < value->array.count; i++) {
            M_PRINT("%s", i == 0 ? "" : ",");
            M_TestJsonDump(&value->array.items[i], buffer, size, used);
        }
        M_PRINT("]");
        break;
    case JSON_OBJECT:
        M_PRINT("{");
        for (int32_t i = 0; i < value->object.count; i++) {
            const JSON_MEMBER *const member = &value->object.members[i];
            M_PRINT("%s\"%s\":", i == 0 ? "" : ",", member->key);
            M_TestJsonDump(member->value, buffer, size, used);
        }
        M_PRINT("}");
        break;
    }
#undef M_PRINT
}

// Holds a JSON text that a test builds piece by piece, with a NUL after it.
// Array_Free releases it.
typedef ARRAY(char) TEST_JSON_TEXT;

// Appends printf-style formatted text.
PRINTF_FORMAT(2, 3)
[[maybe_unused]] static void TestJson_Append(
    TEST_JSON_TEXT *const text, const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *const piece = Str_FormatV(fmt, args);
    va_end(args);
    if (text->count > 0) {
        text->count--;
    }
    Array_AppendSlice(text, piece, (int32_t)strlen(piece) + 1);
    Mem_Free(piece);
}

// Returns value written as compact JSON, with strings and keys unescaped,
// or "(null)" for nullptr. The string is valid until the next call.
[[maybe_unused]] static const char *TestJson_Dump(const JSON_VALUE *const value)
{
    static char buffer[4096];
    if (value == nullptr) {
        return "(null)";
    }
    size_t used = 0;
    M_TestJsonDump(value, buffer, sizeof(buffer), &used);
    return buffer;
}
