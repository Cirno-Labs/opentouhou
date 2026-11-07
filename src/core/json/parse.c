// Parses JSON5 text into a JSON_VALUE tree. The parser is derived from the
// one in TRX (https://github.com/LostArtefacts/TRX, GPL-3.0), which is
// derived from json.h by Neil Henning (https://github.com/sheredom/json.h),
// released into the public domain.
//
// The text is parsed twice. The first pass checks it and counts the values,
// the members, the string bytes and the children of every container. The
// second pass, which cannot fail, fills one block of exactly that size.

#include "core/json.h"

#include "core/base/array.h"
#include "core/base/assert.h"
#include "core/base/string.h"

#include <math.h>
#include <stdarg.h>
#include <stdckdint.h>
#include <stdlib.h>
#include <string.h>

#define M_NUMBER_BUFFER_SIZE 64

typedef ARRAY(int32_t) COUNT_ARRAY;

typedef struct {
    const uint8_t *text;
    size_t size;
    size_t offset;
    int32_t depth;
    bool building;
    // Holds the child count of every array and object, in the order in which
    // the containers open. The first pass appends to it and the second pass
    // reads it from next_count onwards.
    COUNT_ARRAY counts;
    int32_t next_count;
    size_t value_count;
    size_t member_count;
    size_t char_count;
    // Point to the free part of each region of the block in the second pass.
    JSON_VALUE *values;
    JSON_MEMBER *members;
    char *chars;
} PARSER;

static int M_Peek(const PARSER *const parser, const size_t ahead)
{
    const size_t offset = parser->offset + ahead;
    return offset < parser->size ? parser->text[offset] : -1;
}

static bool M_IsDigit(const int c)
{
    return c >= '0' && c <= '9';
}

static int M_HexDigit(const int c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static bool M_IsNameStart(const int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
        || c == '$';
}

static bool M_IsNameChar(const int c)
{
    return M_IsNameStart(c) || M_IsDigit(c);
}

PRINTF_FORMAT(3, 4)
static RESULT M_Fail(
    const PARSER *const parser, size_t offset, const char *const fmt, ...)
{
    offset = MIN(offset, parser->size);
    size_t line = 1;
    size_t line_start = 0;
    for (size_t i = 0; i < offset; i++) {
        if (parser->text[i] == '\n') {
            line++;
            line_start = i + 1;
        }
    }
    size_t column = 1;
    for (size_t i = line_start; i < offset; i++) {
        if ((parser->text[i] & 0xC0) != 0x80) {
            column++;
        }
    }

    va_list args;
    va_start(args, fmt);
    char *const message = Str_FormatV(fmt, args);
    va_end(args);
    const RESULT result =
        FAIL("line %zu, column %zu: %s", line, column, message);
    Mem_Free(message);
    return result;
}

static RESULT M_FailExpected(
    const PARSER *const parser, const char *const expected)
{
    if (parser->offset >= parser->size) {
        return M_Fail(parser, parser->offset, "unexpected end of text");
    }
    return M_Fail(parser, parser->offset, "expected %s", expected);
}

static RESULT M_SkipSpace(PARSER *const parser)
{
    for (;;) {
        const int c = M_Peek(parser, 0);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            parser->offset++;
        } else if (c == '/' && M_Peek(parser, 1) == '/') {
            while (parser->offset < parser->size
                   && parser->text[parser->offset] != '\n') {
                parser->offset++;
            }
        } else if (c == '/' && M_Peek(parser, 1) == '*') {
            const size_t start = parser->offset;
            parser->offset += 2;
            while (!(M_Peek(parser, 0) == '*' && M_Peek(parser, 1) == '/')) {
                if (parser->offset >= parser->size) {
                    return M_Fail(parser, start, "unterminated comment");
                }
                parser->offset++;
            }
            parser->offset += 2;
        } else {
            return OK;
        }
    }
}

// Returns the length of the UTF-8 sequence at the start of bytes, or 0 if it
// is not a valid one. Overlong forms, surrogates and code points above
// U+10FFFF are invalid.
static size_t M_Utf8Length(const uint8_t *const bytes, const size_t available)
{
    const uint8_t lead = bytes[0];
    size_t length;
    uint8_t second_min = 0x80;
    uint8_t second_max = 0xBF;
    if (lead >= 0xC2 && lead <= 0xDF) {
        length = 2;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        length = 3;
        second_min = lead == 0xE0 ? 0xA0 : 0x80;
        second_max = lead == 0xED ? 0x9F : 0xBF;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        length = 4;
        second_min = lead == 0xF0 ? 0x90 : 0x80;
        second_max = lead == 0xF4 ? 0x8F : 0xBF;
    } else {
        return 0;
    }
    if (length > available || bytes[1] < second_min || bytes[1] > second_max) {
        return 0;
    }
    for (size_t i = 2; i < length; i++) {
        if ((bytes[i] & 0xC0) != 0x80) {
            return 0;
        }
    }
    return length;
}

static void M_Emit(
    char *const out, size_t *const size, const uint32_t code_point)
{
    uint8_t bytes[4];
    size_t length;
    if (code_point < 0x80) {
        bytes[0] = (uint8_t)code_point;
        length = 1;
    } else if (code_point < 0x800) {
        bytes[0] = (uint8_t)(0xC0 | (code_point >> 6));
        bytes[1] = (uint8_t)(0x80 | (code_point & 0x3F));
        length = 2;
    } else if (code_point < 0x10000) {
        bytes[0] = (uint8_t)(0xE0 | (code_point >> 12));
        bytes[1] = (uint8_t)(0x80 | ((code_point >> 6) & 0x3F));
        bytes[2] = (uint8_t)(0x80 | (code_point & 0x3F));
        length = 3;
    } else {
        bytes[0] = (uint8_t)(0xF0 | (code_point >> 18));
        bytes[1] = (uint8_t)(0x80 | ((code_point >> 12) & 0x3F));
        bytes[2] = (uint8_t)(0x80 | ((code_point >> 6) & 0x3F));
        bytes[3] = (uint8_t)(0x80 | (code_point & 0x3F));
        length = 4;
    }
    if (out != nullptr) {
        memcpy(out + *size, bytes, length);
    }
    *size += length;
}

// Reads the four hex digits of a \u escape that starts at offset, and moves
// past it. Returns false if the escape is not "\u" and four hex digits.
static bool M_ReadUnicodeEscape(PARSER *const parser, uint32_t *const unit)
{
    if (M_Peek(parser, 0) != '\\' || M_Peek(parser, 1) != 'u') {
        return false;
    }
    *unit = 0;
    for (size_t i = 2; i < 6; i++) {
        const int digit = M_HexDigit(M_Peek(parser, i));
        if (digit < 0) {
            return false;
        }
        *unit = *unit << 4 | (uint32_t)digit;
    }
    parser->offset += 6;
    return true;
}

static RESULT M_ParseEscape(
    PARSER *const parser, char *const out, size_t *const size)
{
    const size_t start = parser->offset;
    const int c = M_Peek(parser, 1);
    char simple;
    switch (c) {
    case '"':
    case '\'':
    case '\\':
    case '/':
        simple = (char)c;
        break;
    case 'b':
        simple = '\b';
        break;
    case 'f':
        simple = '\f';
        break;
    case 'n':
        simple = '\n';
        break;
    case 'r':
        simple = '\r';
        break;
    case 't':
        simple = '\t';
        break;
    case '\n':
        parser->offset += 2;
        return OK;
    case '\r':
        parser->offset += M_Peek(parser, 2) == '\n' ? 3 : 2;
        return OK;
    case 'u': {
        uint32_t code_point;
        if (!M_ReadUnicodeEscape(parser, &code_point)) {
            return M_Fail(parser, start, "invalid \\u escape");
        }
        if (code_point >= 0xD800 && code_point <= 0xDBFF) {
            uint32_t low;
            const size_t low_start = parser->offset;
            if (!M_ReadUnicodeEscape(parser, &low) || low < 0xDC00
                || low > 0xDFFF) {
                return M_Fail(
                    parser, low_start, "expected a low surrogate \\u escape");
            }
            code_point =
                0x10000 + ((code_point - 0xD800) << 10) + (low - 0xDC00);
        } else if (code_point >= 0xDC00 && code_point <= 0xDFFF) {
            return M_Fail(parser, start, "unpaired low surrogate \\u escape");
        }
        M_Emit(out, size, code_point);
        return OK;
    }
    case -1:
        return M_Fail(parser, parser->size, "unexpected end of text");
    default:
        return M_Fail(parser, start, "invalid escape sequence");
    }
    M_Emit(out, size, (uint8_t)simple);
    parser->offset += 2;
    return OK;
}

// Parses a string in double or single quotes. In the first pass, out is
// nullptr and only the size is counted.
static RESULT M_ParseString(
    PARSER *const parser, char *const out, size_t *const size)
{
    const size_t start = parser->offset;
    const uint8_t quote = parser->text[parser->offset];
    parser->offset++;
    *size = 0;
    for (;;) {
        if (parser->offset >= parser->size) {
            return M_Fail(parser, start, "unterminated string");
        }
        const uint8_t c = parser->text[parser->offset];
        if (c == quote) {
            parser->offset++;
            return OK;
        }
        if (c == '\\') {
            MUST(M_ParseEscape(parser, out, size));
        } else if (c == '\n' || c == '\r') {
            return M_Fail(parser, parser->offset, "line break in string");
        } else if (c < 0x20 && c != '\t') {
            return M_Fail(
                parser, parser->offset,
                "unescaped control character in string");
        } else if (c < 0x80) {
            if (out != nullptr) {
                out[*size] = (char)c;
            }
            (*size)++;
            parser->offset++;
        } else {
            const size_t length = M_Utf8Length(
                &parser->text[parser->offset], parser->size - parser->offset);
            if (length == 0) {
                return M_Fail(
                    parser, parser->offset, "invalid UTF-8 in string");
            }
            if (out != nullptr) {
                memcpy(out + *size, &parser->text[parser->offset], length);
            }
            *size += length;
            parser->offset += length;
        }
    }
}

static RESULT M_ParseStringValue(
    PARSER *const parser, const char **const data, size_t *const size)
{
    char *const out = parser->building ? parser->chars : nullptr;
    MUST(M_ParseString(parser, out, size));
    if (parser->building) {
        out[*size] = '\0';
        parser->chars += *size + 1;
        *data = out;
    } else {
        parser->char_count += *size + 1;
    }
    return OK;
}

static RESULT M_ParseKey(PARSER *const parser, const char **const key)
{
    const int c = M_Peek(parser, 0);
    size_t size;
    if (c == '"' || c == '\'') {
        return M_ParseStringValue(parser, key, &size);
    }
    if (!M_IsNameStart(c)) {
        return M_FailExpected(parser, "a key");
    }
    const size_t start = parser->offset;
    while (M_IsNameChar(M_Peek(parser, 0))) {
        parser->offset++;
    }
    size = parser->offset - start;
    if (parser->building) {
        memcpy(parser->chars, &parser->text[start], size);
        parser->chars[size] = '\0';
        *key = parser->chars;
        parser->chars += size + 1;
    } else {
        parser->char_count += size + 1;
    }
    return OK;
}

// Takes the literal word at the current offset if it is there and is not
// the start of a longer name.
static bool M_TakeWord(PARSER *const parser, const char *const word)
{
    const size_t length = strlen(word);
    if (parser->size - parser->offset < length
        || memcmp(&parser->text[parser->offset], word, length) != 0
        || M_IsNameChar(M_Peek(parser, length))) {
        return false;
    }
    parser->offset += length;
    return true;
}

// Stores a magnitude read from the digits as an int64_t, applying the
// sign. Returns false if it does not fit.
static bool M_ToInteger(
    const uint64_t magnitude, const bool negative, int64_t *const integer)
{
    if (negative && magnitude == (uint64_t)INT64_MAX + 1) {
        *integer = INT64_MIN;
        return true;
    }
    if (magnitude > (uint64_t)INT64_MAX) {
        return false;
    }
    *integer = negative ? -(int64_t)magnitude : (int64_t)magnitude;
    return true;
}

// Adds the decimal or hexadecimal digits at the current offset to
// magnitude. Sets overflow if magnitude no longer fits in 64 bits. Returns
// the number of digits.
static size_t M_TakeDigits(
    PARSER *const parser, const int base, uint64_t *const magnitude,
    bool *const overflow)
{
    size_t count = 0;
    for (;;) {
        const int c = M_Peek(parser, 0);
        int digit = M_HexDigit(c);
        if (base == 10 && !M_IsDigit(c)) {
            digit = -1;
        }
        if (digit < 0) {
            return count;
        }
        if (ckd_mul(magnitude, *magnitude, (uint64_t)base)
            || ckd_add(magnitude, *magnitude, (uint64_t)digit)) {
            *overflow = true;
        }
        parser->offset++;
        count++;
    }
}

// Converts the decimal number from start to the current offset with strtod,
// which needs a NUL-terminated copy.
static double M_ConvertDecimal(const PARSER *const parser, const size_t start)
{
    const size_t length = parser->offset - start;
    char buffer[M_NUMBER_BUFFER_SIZE];
    char *const copy = length < sizeof(buffer) ? buffer : Mem_Alloc(length + 1);
    memcpy(copy, &parser->text[start], length);
    copy[length] = '\0';
    const double value = strtod(copy, nullptr);
    if (copy != buffer) {
        Mem_Free(copy);
    }
    return value;
}

static RESULT M_ParseNumber(PARSER *const parser, JSON_VALUE *const out)
{
    const size_t start = parser->offset;
    const int sign = M_Peek(parser, 0);
    const bool negative = sign == '-';
    if (sign == '-' || sign == '+') {
        parser->offset++;
    }

    double value;
    int64_t integer = 0;
    bool is_integer = false;
    if (M_TakeWord(parser, "Infinity")) {
        value = negative ? -INFINITY : INFINITY;
    } else if (M_TakeWord(parser, "NaN")) {
        value = NAN;
    } else if (
        M_Peek(parser, 0) == '0'
        && (M_Peek(parser, 1) == 'x' || M_Peek(parser, 1) == 'X')) {
        parser->offset += 2;
        uint64_t magnitude = 0;
        bool overflow = false;
        if (M_TakeDigits(parser, 16, &magnitude, &overflow) == 0) {
            return M_Fail(parser, start, "invalid number");
        }
        if (overflow || !M_ToInteger(magnitude, negative, &integer)) {
            return M_Fail(parser, start, "number out of range");
        }
        is_integer = true;
        value = (double)integer;
    } else {
        uint64_t magnitude = 0;
        bool overflow = false;
        const bool leading_zero = M_Peek(parser, 0) == '0';
        const size_t int_digits =
            M_TakeDigits(parser, 10, &magnitude, &overflow);
        if (leading_zero && int_digits > 1) {
            return M_Fail(parser, start, "invalid number");
        }
        size_t frac_digits = 0;
        bool is_decimal = false;
        if (M_Peek(parser, 0) == '.') {
            parser->offset++;
            is_decimal = true;
            while (M_IsDigit(M_Peek(parser, 0))) {
                parser->offset++;
                frac_digits++;
            }
        }
        if (int_digits == 0 && frac_digits == 0) {
            return M_Fail(parser, start, "invalid number");
        }
        if (M_Peek(parser, 0) == 'e' || M_Peek(parser, 0) == 'E') {
            parser->offset++;
            is_decimal = true;
            if (M_Peek(parser, 0) == '+' || M_Peek(parser, 0) == '-') {
                parser->offset++;
            }
            if (!M_IsDigit(M_Peek(parser, 0))) {
                return M_Fail(parser, start, "invalid number");
            }
            while (M_IsDigit(M_Peek(parser, 0))) {
                parser->offset++;
            }
        }
        value = M_ConvertDecimal(parser, start);
        if (isinf(value)) {
            return M_Fail(parser, start, "number out of range");
        }
        is_integer = !is_decimal && !overflow
            && M_ToInteger(magnitude, negative, &integer);
    }

    const int next = M_Peek(parser, 0);
    if (M_IsNameChar(next) || next == '.') {
        return M_Fail(parser, start, "invalid number");
    }
    if (out != nullptr) {
        out->type = JSON_NUMBER;
        out->number.value = value;
        out->number.integer = integer;
        out->number.is_integer = is_integer;
    }
    return OK;
}

static RESULT M_ParseValue(PARSER *parser, JSON_VALUE *out);

// Opens the array or object at the current offset, if the depth allows it.
// *slot receives the index of its child count in parser->counts.
static RESULT M_OpenContainer(PARSER *const parser, int32_t *const slot)
{
    if (parser->depth >= JSON_MAX_DEPTH) {
        return M_Fail(
            parser, parser->offset, "nesting deeper than %d levels",
            JSON_MAX_DEPTH);
    }
    parser->depth++;
    parser->offset++;
    if (parser->building) {
        *slot = parser->next_count++;
    } else {
        *slot = parser->counts.count;
        Array_Append(&parser->counts, 0);
    }
    return OK;
}

static RESULT M_CountChild(PARSER *const parser, int32_t *const count)
{
    if (*count == INT32_MAX) {
        return M_Fail(parser, parser->offset, "too many items");
    }
    (*count)++;
    return OK;
}

static void M_CloseContainer(
    PARSER *const parser, const int32_t slot, const int32_t count,
    const bool is_object)
{
    parser->offset++;
    parser->depth--;
    if (parser->building) {
        ASSERT(parser->counts.items[slot] == count);
        return;
    }
    parser->counts.items[slot] = count;
    parser->value_count += (size_t)count;
    if (is_object) {
        parser->member_count += (size_t)count;
    }
}

// Moves past the ',' after an item, or reports whether the closing bracket
// follows instead. A ',' before the closing bracket is allowed.
static RESULT M_NextItem(
    PARSER *const parser, const char close, const char *const expected,
    bool *const closed)
{
    MUST(M_SkipSpace(parser));
    const int c = M_Peek(parser, 0);
    if (c == close) {
        *closed = true;
        return OK;
    }
    if (c != ',') {
        return M_FailExpected(parser, expected);
    }
    parser->offset++;
    MUST(M_SkipSpace(parser));
    *closed = M_Peek(parser, 0) == close;
    return OK;
}

static RESULT M_ParseArray(PARSER *const parser, JSON_VALUE *const out)
{
    int32_t slot;
    MUST(M_OpenContainer(parser, &slot));
    JSON_VALUE *items = nullptr;
    if (parser->building) {
        const int32_t capacity = parser->counts.items[slot];
        items = parser->values;
        parser->values += capacity;
        out->type = JSON_ARRAY;
        out->array.items = items;
        out->array.count = capacity;
    }

    int32_t count = 0;
    MUST(M_SkipSpace(parser));
    bool closed = M_Peek(parser, 0) == ']';
    while (!closed) {
        MUST(M_ParseValue(parser, items != nullptr ? &items[count] : nullptr));
        MUST(M_CountChild(parser, &count));
        MUST(M_NextItem(parser, ']', "',' or ']'", &closed));
    }
    M_CloseContainer(parser, slot, count, false);
    return OK;
}

static RESULT M_ParseObject(PARSER *const parser, JSON_VALUE *const out)
{
    int32_t slot;
    MUST(M_OpenContainer(parser, &slot));
    JSON_VALUE *values = nullptr;
    JSON_MEMBER *members = nullptr;
    if (parser->building) {
        const int32_t capacity = parser->counts.items[slot];
        values = parser->values;
        parser->values += capacity;
        members = parser->members;
        parser->members += capacity;
        out->type = JSON_OBJECT;
        out->object.members = members;
        out->object.count = capacity;
    }

    int32_t count = 0;
    MUST(M_SkipSpace(parser));
    bool closed = M_Peek(parser, 0) == '}';
    while (!closed) {
        const char *key = nullptr;
        MUST(M_ParseKey(parser, &key));
        MUST(M_SkipSpace(parser));
        if (M_Peek(parser, 0) != ':') {
            return M_FailExpected(parser, "':'");
        }
        parser->offset++;
        JSON_VALUE *value = nullptr;
        if (members != nullptr) {
            value = &values[count];
            members[count] = (JSON_MEMBER) { .key = key, .value = value };
        }
        MUST(M_ParseValue(parser, value));
        MUST(M_CountChild(parser, &count));
        MUST(M_NextItem(parser, '}', "',' or '}'", &closed));
    }
    M_CloseContainer(parser, slot, count, true);
    return OK;
}

static RESULT M_ParseValue(PARSER *const parser, JSON_VALUE *const out)
{
    MUST(M_SkipSpace(parser));
    const int c = M_Peek(parser, 0);
    if (c == '{') {
        return M_ParseObject(parser, out);
    }
    if (c == '[') {
        return M_ParseArray(parser, out);
    }
    if (c == '"' || c == '\'') {
        const char *data = nullptr;
        size_t size;
        MUST(M_ParseStringValue(parser, &data, &size));
        if (out != nullptr) {
            out->type = JSON_STRING;
            out->string.data = data;
            out->string.size = size;
        }
        return OK;
    }
    if (M_IsDigit(c) || c == '-' || c == '+' || c == '.' || c == 'I'
        || c == 'N') {
        return M_ParseNumber(parser, out);
    }
    if (M_TakeWord(parser, "true") || M_TakeWord(parser, "false")) {
        if (out != nullptr) {
            out->type = JSON_BOOL;
            out->boolean = c == 't';
        }
        return OK;
    }
    if (M_TakeWord(parser, "null")) {
        if (out != nullptr) {
            out->type = JSON_NULL;
        }
        return OK;
    }
    return M_FailExpected(parser, "a value");
}

static RESULT M_ParseRoot(PARSER *const parser, JSON_VALUE *const out)
{
    parser->offset = 0;
    if (parser->size >= 3 && memcmp(parser->text, "\xEF\xBB\xBF", 3) == 0) {
        parser->offset = 3;
    }
    MUST(M_ParseValue(parser, out));
    MUST(M_SkipSpace(parser));
    if (parser->offset < parser->size) {
        return M_Fail(
            parser, parser->offset, "unexpected text after the value");
    }
    return OK;
}

RESULT Json_Parse(
    const char *const text, const size_t size, const ALLOCATOR *const allocator,
    JSON_VALUE **const out)
{
    ASSERT(text != nullptr || size == 0);
    *out = nullptr;
    PARSER parser = {
        .text = (const uint8_t *)text,
        .size = size,
        .value_count = 1,
    };
    Array_Init(&parser.counts, &g_HeapAllocator);
    const RESULT result = M_ParseRoot(&parser, nullptr);
    if (!IS_OK(result)) {
        Array_Free(&parser.counts);
        return result;
    }

    const size_t values_size =
        Mem_ArrSize(parser.value_count, sizeof(JSON_VALUE));
    const size_t members_size =
        Mem_ArrSize(parser.member_count, sizeof(JSON_MEMBER));
    size_t total;
    if (ckd_add(&total, values_size, members_size)
        || ckd_add(&total, total, parser.char_count)) {
        LOG_FATAL("JSON text of %zu bytes is too large", size);
    }
    char *const block = Allocator_Alloc(allocator, total);
    JSON_VALUE *const root = (JSON_VALUE *)block;
    parser.building = true;
    parser.values = root + 1;
    parser.members = (JSON_MEMBER *)(block + values_size);
    parser.chars = block + values_size + members_size;
    const bool built = Result_Log(M_ParseRoot(&parser, root), LOG_LEVEL_ERROR);
    ASSERT(built);
    Array_Free(&parser.counts);
    *out = root;
    return OK;
}
