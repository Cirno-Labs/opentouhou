// SPDX-License-Identifier: GPL-3.0-only

#include "core/base/arena.h"
#include "core/base/memory.h"
#include "core/json.h"

#include "test.h"
#include "test_allocator.h"

#include <math.h>

static ARENA m_Arena;

[[gnu::destructor]] static void M_FreeArena(void)
{
    Arena_Free(&m_Arena);
}

static const JSON_VALUE *M_ParseSized(const char *const text, const size_t size)
{
    if (m_Arena.base == nullptr) {
        Arena_Init(&m_Arena, "json test", 64 << 20);
    }
    Arena_Reset(&m_Arena);
    JSON_VALUE *value = nullptr;
    const RESULT result =
        Json_Parse(text, size, Arena_Allocator(&m_Arena), &value);
    if (!IS_OK(result)) {
        printf("    %s\n", result.message);
    }
    return Result_Ignore(result) ? value : nullptr;
}

static const JSON_VALUE *M_Parse(const char *const text)
{
    return M_ParseSized(text, strlen(text));
}

// Returns the failure message of parsing text, or "(success)". The string
// is valid until the next call.
static const char *M_Error(const char *const text, const size_t size)
{
    static char message[256];
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    JSON_VALUE *value = &(JSON_VALUE) {};
    const RESULT result = Json_Parse(text, size, &test.allocator, &value);
    if (IS_OK(result)) {
        Allocator_Free(&test.allocator, value, test.live_bytes);
        return "(success)";
    }
    snprintf(message, sizeof(message), "%s", result.message);
    Result_Ignore(result);
    if (value != nullptr || test.live_blocks != 0) {
        return "(failure with output or memory left)";
    }
    return message;
}

#define CHECK_ERROR(text_, expected_)                                          \
    CHECK_EQ_STR(M_Error((text_), sizeof(text_) - 1), (expected_))

static bool M_IsNumber(
    const JSON_VALUE *const value, const double expected, const bool is_integer)
{
    return value != nullptr && value->type == JSON_NUMBER
        && value->number.value == expected
        && value->number.is_integer == is_integer;
}

static bool M_IsInteger(const JSON_VALUE *const value, const int64_t expected)
{
    return value != nullptr && value->type == JSON_NUMBER
        && value->number.is_integer && value->number.integer == expected;
}

static bool M_IsString(
    const JSON_VALUE *const value, const char *const expected)
{
    return value != nullptr && value->type == JSON_STRING
        && value->string.size == strlen(expected)
        && strcmp(value->string.data, expected) == 0;
}

TEST(parses_the_scalar_types)
{
    const JSON_VALUE *value = M_Parse("null");
    CHECK(value != nullptr && value->type == JSON_NULL);
    value = M_Parse("true");
    CHECK(value != nullptr && value->type == JSON_BOOL && value->boolean);
    value = M_Parse(" false ");
    CHECK(value != nullptr && value->type == JSON_BOOL && !value->boolean);
    CHECK(M_IsInteger(M_Parse("42"), 42));
    CHECK(M_IsString(M_Parse("\"text\""), "text"));
}

TEST(parses_a_strict_json_document)
{
    const JSON_VALUE *const root = M_Parse(
        "{\n"
        "  \"name\": \"th06\",\n"
        "  \"version\": 1.02,\n"
        "  \"stages\": [1, 2, 3, 4, 5, 6],\n"
        "  \"extra\": {\"enabled\": false, \"boss\": null},\n"
        "  \"empty_array\": [],\n"
        "  \"empty_object\": {}\n"
        "}\n");
    CHECK(root != nullptr && root->type == JSON_OBJECT);
    if (root == nullptr) {
        return;
    }
    CHECK_EQ_INT(root->object.count, 6);
    const JSON_MEMBER *const members = root->object.members;
    CHECK_EQ_STR(members[0].key, "name");
    CHECK(M_IsString(members[0].value, "th06"));
    CHECK_EQ_STR(members[1].key, "version");
    CHECK(M_IsNumber(members[1].value, 1.02, false));
    CHECK_EQ_STR(members[2].key, "stages");
    CHECK(members[2].value->type == JSON_ARRAY);
    CHECK_EQ_INT(members[2].value->array.count, 6);
    bool stages_right = true;
    for (int32_t i = 0; i < 6; i++) {
        stages_right = stages_right
            && M_IsInteger(&members[2].value->array.items[i], i + 1);
    }
    CHECK(stages_right);
    const JSON_VALUE *const extra = members[3].value;
    CHECK(extra->type == JSON_OBJECT);
    CHECK_EQ_INT(extra->object.count, 2);
    CHECK_EQ_STR(extra->object.members[0].key, "enabled");
    CHECK(extra->object.members[0].value->type == JSON_BOOL);
    CHECK(!extra->object.members[0].value->boolean);
    CHECK(extra->object.members[1].value->type == JSON_NULL);
    CHECK(members[4].value->type == JSON_ARRAY);
    CHECK_EQ_INT(members[4].value->array.count, 0);
    CHECK(members[5].value->type == JSON_OBJECT);
    CHECK_EQ_INT(members[5].value->object.count, 0);
}

TEST(parses_nested_values)
{
    const JSON_VALUE *const root =
        M_Parse("[[1, [2, [3]]], {\"a\": [{\"b\": {\"c\": [true]}}]}]");
    CHECK(root != nullptr && root->type == JSON_ARRAY);
    if (root == nullptr) {
        return;
    }
    CHECK_EQ_INT(root->array.count, 2);
    const JSON_VALUE *const first = &root->array.items[0];
    CHECK(M_IsInteger(&first->array.items[0], 1));
    CHECK(M_IsInteger(&first->array.items[1].array.items[0], 2));
    CHECK(M_IsInteger(&first->array.items[1].array.items[1].array.items[0], 3));
    const JSON_VALUE *const a = root->array.items[1].object.members[0].value;
    CHECK_EQ_STR(root->array.items[1].object.members[0].key, "a");
    const JSON_VALUE *const b = a->array.items[0].object.members[0].value;
    const JSON_VALUE *const c = b->object.members[0].value;
    CHECK_EQ_STR(b->object.members[0].key, "c");
    CHECK(c->type == JSON_ARRAY && c->array.count == 1);
    CHECK(c->array.items[0].type == JSON_BOOL && c->array.items[0].boolean);
}

TEST(keeps_duplicate_keys_in_order)
{
    const JSON_VALUE *const root = M_Parse("{\"a\": 1, \"b\": 2, \"a\": 3}");
    CHECK(root != nullptr && root->object.count == 3);
    if (root == nullptr) {
        return;
    }
    CHECK_EQ_STR(root->object.members[0].key, "a");
    CHECK_EQ_STR(root->object.members[2].key, "a");
    CHECK(M_IsInteger(root->object.members[2].value, 3));
}

TEST(skips_comments)
{
    const JSON_VALUE *const root = M_Parse(
        "// leading comment\n"
        "{ /* before a key */ \"a\" /* before the colon */ : // after it\n"
        "  1 /* * / ** */, \"b\": [ // in an array\n"
        "  2 ] } // trailing comment without a line break");
    CHECK(root != nullptr && root->object.count == 2);
    if (root == nullptr) {
        return;
    }
    CHECK(M_IsInteger(root->object.members[0].value, 1));
    CHECK(M_IsInteger(&root->object.members[1].value->array.items[0], 2));
}

TEST(accepts_trailing_commas)
{
    const JSON_VALUE *value = M_Parse("[1, 2, ]");
    CHECK(value != nullptr && value->array.count == 2);
    value = M_Parse("{\"a\": 1, }");
    CHECK(value != nullptr && value->object.count == 1);
    value = M_Parse("[[],{},]");
    CHECK(value != nullptr && value->array.count == 2);
}

TEST(accepts_unquoted_keys)
{
    const JSON_VALUE *const root =
        M_Parse("{name: 1, _private: 2, $dollar: 3, mixed_Case9: 4, true: 5}");
    CHECK(root != nullptr && root->object.count == 5);
    if (root == nullptr) {
        return;
    }
    CHECK_EQ_STR(root->object.members[0].key, "name");
    CHECK_EQ_STR(root->object.members[1].key, "_private");
    CHECK_EQ_STR(root->object.members[2].key, "$dollar");
    CHECK_EQ_STR(root->object.members[3].key, "mixed_Case9");
    CHECK_EQ_STR(root->object.members[4].key, "true");
    CHECK(M_IsInteger(root->object.members[4].value, 5));
}

TEST(accepts_single_quoted_strings)
{
    CHECK(M_IsString(M_Parse("'single'"), "single"));
    CHECK(M_IsString(M_Parse("'say \"hi\"'"), "say \"hi\""));
    CHECK(M_IsString(M_Parse("'it\\'s'"), "it's"));
    CHECK(M_IsString(M_Parse("\"it\\'s\""), "it's"));
    const JSON_VALUE *const root = M_Parse("{'key': 'value'}");
    CHECK(root != nullptr && root->object.count == 1);
    if (root != nullptr) {
        CHECK_EQ_STR(root->object.members[0].key, "key");
        CHECK(M_IsString(root->object.members[0].value, "value"));
    }
}

TEST(continues_strings_after_escaped_line_breaks)
{
    CHECK(M_IsString(M_Parse("\"first \\\nsecond\""), "first second"));
    CHECK(M_IsString(M_Parse("'first \\\r\nsecond'"), "first second"));
    CHECK(M_IsString(M_Parse("'first \\\rsecond'"), "first second"));
}

TEST(parses_integers_exactly)
{
    CHECK(M_IsInteger(M_Parse("0"), 0));
    CHECK(M_IsInteger(M_Parse("-0"), 0));
    CHECK(M_IsInteger(M_Parse("-17"), -17));
    CHECK(M_IsInteger(M_Parse("+17"), 17));
    CHECK(M_IsInteger(M_Parse("9223372036854775807"), INT64_MAX));
    CHECK(M_IsInteger(M_Parse("-9223372036854775808"), INT64_MIN));
    CHECK(M_IsInteger(M_Parse("9007199254740993"), 9007199254740993));
    const JSON_VALUE *const big = M_Parse("9007199254740993");
    CHECK(big != nullptr && big->number.value == 9007199254740992.0);
}

TEST(parses_integers_beyond_int64_as_numbers)
{
    CHECK(M_IsNumber(M_Parse("9223372036854775808"), 0x1p63, false));
    CHECK(M_IsNumber(M_Parse("-9223372036854775809"), -0x1p63, false));
    CHECK(M_IsNumber(M_Parse("100000000000000000000000"), 1e23, false));
}

TEST(parses_fractions_and_exponents)
{
    CHECK(M_IsNumber(M_Parse("1.5"), 1.5, false));
    CHECK(M_IsNumber(M_Parse("-0.25"), -0.25, false));
    CHECK(M_IsNumber(M_Parse("1.0"), 1.0, false));
    CHECK(M_IsNumber(M_Parse("1e3"), 1000.0, false));
    CHECK(M_IsNumber(M_Parse("2E+2"), 200.0, false));
    CHECK(M_IsNumber(M_Parse("25e-2"), 0.25, false));
    CHECK(M_IsNumber(M_Parse("0.1"), 0.1, false));
    CHECK(M_IsNumber(
        M_Parse("1.7976931348623157e308"), 0x1.fffffffffffffp1023, false));
    CHECK(M_IsNumber(M_Parse("4.9e-324"), 0x1p-1074, false));
    CHECK(M_IsNumber(M_Parse("1e-400"), 0.0, false));
}

TEST(accepts_leading_and_trailing_decimal_points)
{
    CHECK(M_IsNumber(M_Parse(".5"), 0.5, false));
    CHECK(M_IsNumber(M_Parse("5."), 5.0, false));
    CHECK(M_IsNumber(M_Parse("-.5"), -0.5, false));
    CHECK(M_IsNumber(M_Parse("+5."), 5.0, false));
    CHECK(M_IsNumber(M_Parse("5.e1"), 50.0, false));
}

TEST(parses_hexadecimal_integers)
{
    CHECK(M_IsInteger(M_Parse("0x1F"), 31));
    CHECK(M_IsInteger(M_Parse("0XfF"), 255));
    CHECK(M_IsInteger(M_Parse("-0x10"), -16));
    CHECK(M_IsInteger(M_Parse("+0x10"), 16));
    CHECK(M_IsInteger(M_Parse("0xFFFFFFFF"), 0xFFFFFFFF));
    CHECK(M_IsInteger(M_Parse("0x7FFFFFFFFFFFFFFF"), INT64_MAX));
    CHECK(M_IsInteger(M_Parse("-0x8000000000000000"), INT64_MIN));
}

TEST(parses_infinity_and_nan)
{
    const JSON_VALUE *value = M_Parse("Infinity");
    CHECK(value != nullptr && isinf(value->number.value));
    CHECK(value != nullptr && value->number.value > 0);
    value = M_Parse("+Infinity");
    CHECK(value != nullptr && isinf(value->number.value));
    CHECK(value != nullptr && value->number.value > 0);
    value = M_Parse("-Infinity");
    CHECK(value != nullptr && isinf(value->number.value));
    CHECK(value != nullptr && value->number.value < 0);
    value = M_Parse("NaN");
    CHECK(value != nullptr && value->type == JSON_NUMBER);
    CHECK(value != nullptr && isnan(value->number.value));
    CHECK(value != nullptr && !value->number.is_integer);
    value = M_Parse("[-NaN, Infinity]");
    CHECK(value != nullptr && isnan(value->array.items[0].number.value));
    CHECK(value != nullptr && isinf(value->array.items[1].number.value));
}

TEST(decodes_string_escapes)
{
    CHECK(M_IsString(
        M_Parse("\"\\\" \\\\ \\/ \\b \\f \\n \\r \\t\""),
        "\" \\ / \b \f \n \r \t"));
    CHECK(M_IsString(M_Parse("\"\\u0041\\u00e9\\u6771\""), "Aé東"));
    CHECK(M_IsString(M_Parse("\"\\uD83D\\uDD25\""), "\xF0\x9F\x94\xA5"));
    CHECK(M_IsString(M_Parse("\"\\udbff\\udfff\""), "\xF4\x8F\xBF\xBF"));
    CHECK(M_IsString(M_Parse("\"東方紅魔郷\""), "東方紅魔郷"));
    CHECK(M_IsString(M_Parse("\"tab\there\""), "tab\there"));
    CHECK(M_IsString(M_Parse("\"\""), ""));
}

TEST(counts_nul_bytes_in_the_string_size)
{
    const JSON_VALUE *const value = M_Parse("\"a\\u0000b\"");
    CHECK(value != nullptr && value->type == JSON_STRING);
    if (value != nullptr) {
        CHECK_EQ_INT(value->string.size, 3);
        CHECK(memcmp(value->string.data, "a\0b", 4) == 0);
    }
}

TEST(skips_a_byte_order_mark)
{
    CHECK(M_IsInteger(M_Parse("\xEF\xBB\xBF 7"), 7));
}

TEST(parses_text_without_a_terminator)
{
    const char text[] = { '[', '1', ',', '2', ']', 'x' };
    const JSON_VALUE *const value = M_ParseSized(text, 5);
    CHECK(value != nullptr && value->array.count == 2);
}

TEST(builds_the_tree_in_one_block)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    const char text[] = "{\"a\": [1, \"two\", {b: null}], c: 'd'}";
    JSON_VALUE *root = nullptr;
    CHECK(Result_Ignore(
        Json_Parse(text, sizeof(text) - 1, &test.allocator, &root)));
    CHECK(root != nullptr);
    CHECK_EQ_INT(test.live_blocks, 1);
    // 7 values, 3 members, and "a", "two", "b", "c", "d" with terminators.
    CHECK_EQ_INT(
        test.live_bytes, 7 * sizeof(JSON_VALUE) + 3 * sizeof(JSON_MEMBER) + 12);
    Allocator_Free(&test.allocator, root, test.live_bytes);
    CHECK_EQ_INT(test.size_errors, 0);
}

TEST(reports_premature_ends)
{
    CHECK_ERROR("", "line 1, column 1: unexpected end of text");
    CHECK_ERROR("  \n ", "line 2, column 2: unexpected end of text");
    CHECK_ERROR("[1, 2", "line 1, column 6: unexpected end of text");
    CHECK_ERROR("{\"a\": 1,\n", "line 2, column 1: unexpected end of text");
    CHECK_ERROR("{\"a\"", "line 1, column 5: unexpected end of text");
    CHECK_ERROR("{\"a\":", "line 1, column 6: unexpected end of text");
    CHECK_ERROR("\"abc", "line 1, column 1: unterminated string");
    CHECK_ERROR("\"abc\\", "line 1, column 6: unexpected end of text");
    CHECK_ERROR("[1] /* comment", "line 1, column 5: unterminated comment");
}

TEST(reports_missing_separators)
{
    CHECK_ERROR(
        "{\n  \"a\": 1,\n  \"b\": 2\n  \"c\": 3\n}",
        "line 4, column 3: expected ',' or '}'");
    CHECK_ERROR("[1 2]", "line 1, column 4: expected ',' or ']'");
    CHECK_ERROR("{\"a\" 1}", "line 1, column 6: expected ':'");
    CHECK_ERROR("{\"a\": 1 ]", "line 1, column 9: expected ',' or '}'");
}

TEST(reports_bad_keys)
{
    CHECK_ERROR("{1: 2}", "line 1, column 2: expected a key");
    CHECK_ERROR("{: 2}", "line 1, column 2: expected a key");
    CHECK_ERROR("{,}", "line 1, column 2: expected a key");
    CHECK_ERROR("{\"a\": 1,,}", "line 1, column 9: expected a key");
    CHECK_ERROR("{a-b: 1}", "line 1, column 3: expected ':'");
}

TEST(reports_bad_values)
{
    CHECK_ERROR("[,]", "line 1, column 2: expected a value");
    CHECK_ERROR("[1,,2]", "line 1, column 4: expected a value");
    CHECK_ERROR("nul", "line 1, column 1: expected a value");
    CHECK_ERROR("nulls", "line 1, column 1: expected a value");
    CHECK_ERROR("True", "line 1, column 1: expected a value");
    CHECK_ERROR("[undefined]", "line 1, column 2: expected a value");
    CHECK_ERROR("1 2", "line 1, column 3: unexpected text after the value");
    CHECK_ERROR("{} x", "line 1, column 4: unexpected text after the value");
    CHECK_ERROR("[1] / 2", "line 1, column 5: unexpected text after the value");
}

TEST(reports_bad_numbers)
{
    CHECK_ERROR("[01]", "line 1, column 2: invalid number");
    CHECK_ERROR("-", "line 1, column 1: invalid number");
    CHECK_ERROR("[+]", "line 1, column 2: invalid number");
    CHECK_ERROR(".", "line 1, column 1: invalid number");
    CHECK_ERROR("1e", "line 1, column 1: invalid number");
    CHECK_ERROR("1e+", "line 1, column 1: invalid number");
    CHECK_ERROR("1.2.3", "line 1, column 1: invalid number");
    CHECK_ERROR("12abc", "line 1, column 1: invalid number");
    CHECK_ERROR("0x", "line 1, column 1: invalid number");
    CHECK_ERROR("0xG", "line 1, column 1: invalid number");
    CHECK_ERROR("Infinit", "line 1, column 1: invalid number");
    CHECK_ERROR("NaNa", "line 1, column 1: invalid number");
    CHECK_ERROR("--1", "line 1, column 1: invalid number");
    CHECK_ERROR("[1e400]", "line 1, column 2: number out of range");
    CHECK_ERROR("-1e400", "line 1, column 1: number out of range");
    CHECK_ERROR("0x8000000000000000", "line 1, column 1: number out of range");
    CHECK_ERROR("0x10000000000000000", "line 1, column 1: number out of range");
}

TEST(reports_bad_strings)
{
    CHECK_ERROR("\"a\\qb\"", "line 1, column 3: invalid escape sequence");
    CHECK_ERROR("\"\\x41\"", "line 1, column 2: invalid escape sequence");
    CHECK_ERROR("\"\\u12\"", "line 1, column 2: invalid \\u escape");
    CHECK_ERROR("\"\\u12G4\"", "line 1, column 2: invalid \\u escape");
    CHECK_ERROR(
        "\"\\uD83D\"", "line 1, column 8: expected a low surrogate \\u escape");
    CHECK_ERROR(
        "\"\\uD83Dx\"",
        "line 1, column 8: expected a low surrogate \\u escape");
    CHECK_ERROR(
        "\"\\uD83D\\u0041\"",
        "line 1, column 8: expected a low surrogate \\u escape");
    CHECK_ERROR(
        "\"\\uDD25\"", "line 1, column 2: unpaired low surrogate \\u escape");
    CHECK_ERROR("\"a\nb\"", "line 1, column 3: line break in string");
    CHECK_ERROR("'a\rb'", "line 1, column 3: line break in string");
    CHECK_ERROR(
        "\"a\x01\"", "line 1, column 3: unescaped control character in string");
    CHECK_ERROR(
        "\"a\0\"", "line 1, column 3: unescaped control character in string");
    CHECK_ERROR("'abc\"", "line 1, column 1: unterminated string");
}

TEST(reports_invalid_utf8_in_strings)
{
    CHECK_ERROR("\"\x80\"", "line 1, column 2: invalid UTF-8 in string");
    CHECK_ERROR("\"a\xFF\"", "line 1, column 3: invalid UTF-8 in string");
    CHECK_ERROR("\"\xC3\"", "line 1, column 2: invalid UTF-8 in string");
    CHECK_ERROR("\"\xC0\xAF\"", "line 1, column 2: invalid UTF-8 in string");
    CHECK_ERROR(
        "\"\xE0\x80\xAF\"", "line 1, column 2: invalid UTF-8 in string");
    CHECK_ERROR(
        "\"\xED\xA0\x80\"", "line 1, column 2: invalid UTF-8 in string");
    CHECK_ERROR(
        "\"\xF4\x90\x80\x80\"", "line 1, column 2: invalid UTF-8 in string");
    CHECK_ERROR("\"\xE6\x9D\"", "line 1, column 2: invalid UTF-8 in string");
    CHECK_ERROR("{'\xFE': 1}", "line 1, column 3: invalid UTF-8 in string");
}

TEST(counts_columns_in_characters)
{
    CHECK_ERROR("{\"東方\": 1 2}", "line 1, column 10: expected ',' or '}'");
    CHECK_ERROR(
        "[\r\n  1,\r\n  2\r\n  3]", "line 4, column 3: expected ',' or ']'");
    CHECK_ERROR(
        "/* line 1\n line 2 */ [\n\t1 2]",
        "line 3, column 4: expected ',' or ']'");
}

TEST(limits_the_nesting_depth)
{
    const size_t depth = JSON_MAX_DEPTH;
    char *const text = Mem_Alloc(2 * depth + 2);
    memset(text, '[', depth);
    memset(text + depth, ']', depth);
    const JSON_VALUE *value = M_ParseSized(text, 2 * depth);
    CHECK(value != nullptr && value->type == JSON_ARRAY);

    memset(text, '[', depth + 1);
    memset(text + depth + 1, ']', depth + 1);
    CHECK_EQ_STR(
        M_Error(text, 2 * depth + 2),
        "line 1, column 513: nesting deeper than 512 levels");
    Mem_Free(text);
}

TEST(rejects_very_deep_nesting_without_recursing)
{
    const size_t depth = 1000000;
    char *const text = Mem_Alloc(depth * 5);
    for (size_t i = 0; i < depth; i++) {
        memcpy(text + i * 5, "{\"a\":", 5);
    }
    CHECK_EQ_STR(
        M_Error(text, depth * 5),
        "line 1, column 2561: nesting deeper than 512 levels");
    memset(text, '[', depth);
    CHECK_EQ_STR(
        M_Error(text, depth),
        "line 1, column 513: nesting deeper than 512 levels");
    Mem_Free(text);
}
