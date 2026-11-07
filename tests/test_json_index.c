#include "core/base/arena.h"
#include "core/json.h"

#include "test.h"
#include "test_allocator.h"
#include "test_json.h"

#define M_STRING_COUNT 5000

static ARENA m_Arena;

[[gnu::destructor]] static void M_FreeArena(void)
{
    Arena_Free(&m_Arena);
}

static const ALLOCATOR *M_Arena(void)
{
    if (m_Arena.base == nullptr) {
        Arena_Init(&m_Arena, "json test", 16 << 20);
    }
    return Arena_Allocator(&m_Arena);
}

// Returns a parsed object in the form of thcrap's stringdefs.js: string ids
// with their translations, the ids keyed "th06_string_<n>". It has a
// comment, a trailing comma and "th06_title".
static const JSON_VALUE *M_ParseStringdefs(void)
{
    TEST_JSON_TEXT text;
    Array_Init(&text, &g_HeapAllocator);
    TestJson_Append(&text, "{\n\t// Strings of the main menu.\n");
    TestJson_Append(
        &text, "\t\"th06_title\": \"the Embodiment of Scarlet Devil\",\n");
    for (int32_t i = 0; i < M_STRING_COUNT; i++) {
        TestJson_Append(
            &text, "\t\"th06_string_%d\": \"Translated string %d\",\n", i, i);
    }
    TestJson_Append(&text, "}\n");
    const JSON_VALUE *const stringdefs = TestJson_Parse(M_Arena(), text.items);
    Array_Free(&text);
    return stringdefs;
}

TEST(index_finds_every_member_of_a_large_object)
{
    const JSON_VALUE *const stringdefs = M_ParseStringdefs();
    CHECK(stringdefs != nullptr);
    CHECK(
        stringdefs != nullptr
        && stringdefs->object.count == M_STRING_COUNT + 1);

    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    const ALLOCATOR *const alloc = &test.allocator;
    JSON_INDEX strings;
    Json_BuildIndex(stringdefs, alloc, &strings);
    const char *text =
        Json_GetString(Json_IndexGet(&strings, "th06_title"), "");
    CHECK_EQ_STR(text, "the Embodiment of Scarlet Devil");
    CHECK_EQ_INT(strings.count, M_STRING_COUNT + 1);

    bool all_found = true;
    for (int32_t i = 0; i < M_STRING_COUNT; i++) {
        char key[32];
        char expected[32];
        snprintf(key, sizeof(key), "th06_string_%d", i);
        snprintf(expected, sizeof(expected), "Translated string %d", i);
        const JSON_VALUE *const value = Json_IndexGet(&strings, key);
        all_found = all_found && value == Json_Get(stringdefs, key)
            && strcmp(Json_GetString(value, ""), expected) == 0;
    }
    CHECK(all_found);
    CHECK(Json_IndexGet(&strings, "th06_string_5000") == nullptr);
    CHECK(Json_IndexGet(&strings, "") == nullptr);
    CHECK(Json_IndexGet(&strings, nullptr) == nullptr);

    HashMap_Free(&strings);
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.size_errors, 0);
}

TEST(index_keeps_the_last_of_duplicate_keys)
{
    const JSON_VALUE *const root =
        TestJson_Parse(M_Arena(), "{a: 1, b: 2, a: 3, b: {c: 4}}");
    JSON_INDEX index;
    Json_BuildIndex(root, &g_HeapAllocator, &index);
    CHECK_EQ_INT(index.count, 2);
    CHECK_EQ_INT(Json_GetInt(Json_IndexGet(&index, "a"), -1), 3);
    CHECK(Json_IndexGet(&index, "a") == Json_Get(root, "a"));
    CHECK_EQ_INT(Json_GetInt(Json_Get(Json_IndexGet(&index, "b"), "c"), -1), 4);
    HashMap_Free(&index);
}

TEST(index_of_a_non_object_is_empty)
{
    const char *const texts[] = { "[1, 2]", "\"a\"", "null", "{}" };
    for (size_t i = 0; i < ARRAY_SIZE(texts); i++) {
        JSON_INDEX index;
        Json_BuildIndex(
            TestJson_Parse(M_Arena(), texts[i]), &g_HeapAllocator, &index);
        CHECK_EQ_INT(index.count, 0);
        CHECK(Json_IndexGet(&index, "0") == nullptr);
        HashMap_Free(&index);
    }
    JSON_INDEX index;
    Json_BuildIndex(nullptr, &g_HeapAllocator, &index);
    CHECK_EQ_INT(index.count, 0);
    HashMap_Free(&index);
}

TEST(index_get_accepts_nullptr)
{
    CHECK(Json_IndexGet(nullptr, "a") == nullptr);
    CHECK_EQ_STR(Json_GetString(Json_IndexGet(nullptr, "a"), "none"), "none");
}

TEST(index_stays_valid_after_a_merge)
{
    JSON_VALUE *const root =
        TestJson_Parse(M_Arena(), "{\"a\": 1, \"b\": {\"c\": 2}}");
    JSON_INDEX index;
    Json_BuildIndex(root, &g_HeapAllocator, &index);
    Json_Merge(
        root, TestJson_Parse(M_Arena(), "{\"a\": \"one\", \"d\": 4}"),
        M_Arena());
    CHECK_EQ_STR(Json_GetString(Json_IndexGet(&index, "a"), ""), "one");
    CHECK(Json_IndexGet(&index, "d") == nullptr);
    CHECK_EQ_INT(Json_GetInt(Json_Get(root, "d"), -1), 4);
    HashMap_Free(&index);
}
