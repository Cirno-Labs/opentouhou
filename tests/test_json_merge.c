// SPDX-License-Identifier: GPL-3.0-only

#include "core/base/arena.h"
#include "core/json.h"

#include "test.h"
#include "test_allocator.h"
#include "test_json.h"

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

// Returns the result of merging source into target, as compact JSON.
static const char *M_Merge(const char *const target, const char *const source)
{
    JSON_VALUE *const target_value = TestJson_Parse(M_Arena(), target);
    const JSON_VALUE *const source_value = TestJson_Parse(M_Arena(), source);
    Json_Merge(target_value, source_value, M_Arena());
    return TestJson_Dump(target_value);
}

TEST(merge_adds_new_keys_after_the_last_member)
{
    CHECK_EQ_STR(
        M_Merge("{\"a\": 1, \"b\": 2}", "{\"d\": 4, \"c\": 3}"),
        "{\"a\":1,\"b\":2,\"d\":4,\"c\":3}");
    CHECK_EQ_STR(M_Merge("{}", "{\"a\": 1}"), "{\"a\":1}");
    CHECK_EQ_STR(M_Merge("{\"a\": 1}", "{}"), "{\"a\":1}");
}

TEST(merge_replaces_values_in_place)
{
    CHECK_EQ_STR(
        M_Merge("{\"a\": 1, \"b\": 2, \"c\": 3}", "{\"b\": \"two\"}"),
        "{\"a\":1,\"b\":\"two\",\"c\":3}");
    CHECK_EQ_STR(M_Merge("{\"a\": {\"x\": 1}}", "{\"a\": [1]}"), "{\"a\":[1]}");
    CHECK_EQ_STR(
        M_Merge("{\"a\": [1]}", "{\"a\": {\"x\": 1}}"), "{\"a\":{\"x\":1}}");
    CHECK_EQ_STR(
        M_Merge("{\"a\": {\"x\": 1}}", "{\"a\": null}"), "{\"a\":null}");
    CHECK_EQ_STR(M_Merge("{\"a\": true}", "{\"a\": false}"), "{\"a\":false}");
}

TEST(merge_replaces_arrays_without_joining_them)
{
    CHECK_EQ_STR(
        M_Merge("{\"list\": [1, 2, 3]}", "{\"list\": [4]}"), "{\"list\":[4]}");
    CHECK_EQ_STR(
        M_Merge("{\"list\": [1, 2, 3]}", "{\"list\": []}"), "{\"list\":[]}");
}

TEST(merge_merges_nested_objects)
{
    CHECK_EQ_STR(
        M_Merge(
            "{\"a\": {\"b\": {\"c\": 1, \"d\": 2}, \"e\": 3}, \"f\": 4}",
            "{\"a\": {\"b\": {\"d\": 20, \"g\": 5}, \"h\": {\"i\": 6}}}"),
        "{\"a\":{\"b\":{\"c\":1,\"d\":20,\"g\":5},\"e\":3,\"h\":{\"i\":6}},"
        "\"f\":4}");
}

TEST(merge_follows_the_patch_stack_order)
{
    JSON_VALUE *const base = TestJson_Parse(
        M_Arena(), "{\"title\": \"base\", \"fonts\": {\"a\": 1}, \"n\": [1]}");
    const JSON_VALUE *const first =
        TestJson_Parse(M_Arena(), "{\"title\": \"first\", \"fonts\": {b: 2}}");
    const JSON_VALUE *const second =
        TestJson_Parse(M_Arena(), "{\"fonts\": {\"a\": 3}, \"n\": [2, 3]}");
    Json_Merge(base, first, M_Arena());
    Json_Merge(base, second, M_Arena());
    CHECK_EQ_STR(
        TestJson_Dump(base),
        "{\"title\":\"first\",\"fonts\":{\"a\":3,\"b\":2},\"n\":[2,3]}");
}

TEST(merge_uses_the_last_of_duplicate_keys)
{
    CHECK_EQ_STR(
        M_Merge("{\"a\": 1, \"a\": 2}", "{\"a\": 3}"), "{\"a\":1,\"a\":3}");
    CHECK_EQ_STR(
        M_Merge("{\"a\": 1}", "{\"b\": 1, \"b\": {\"x\": 2}}"),
        "{\"a\":1,\"b\":{\"x\":2}}");
    CHECK_EQ_STR(
        M_Merge("{}", "{\"b\": {\"x\": 1}, \"b\": {\"y\": 2}}"),
        "{\"b\":{\"x\":1,\"y\":2}}");
}

TEST(merge_ignores_values_that_are_not_objects)
{
    CHECK_EQ_STR(M_Merge("[1, 2]", "{\"a\": 1}"), "[1,2]");
    CHECK_EQ_STR(M_Merge("{\"a\": 1}", "[1, 2]"), "{\"a\":1}");
    CHECK_EQ_STR(M_Merge("{\"a\": 1}", "\"text\""), "{\"a\":1}");
    JSON_VALUE *const target = TestJson_Parse(M_Arena(), "{\"a\": 1}");
    Json_Merge(target, nullptr, M_Arena());
    Json_Merge(nullptr, target, M_Arena());
    CHECK_EQ_STR(TestJson_Dump(target), "{\"a\":1}");
}

TEST(merge_copies_everything_it_takes_from_source)
{
    JSON_VALUE *const target = TestJson_Parse(
        M_Arena(), "{\"keep\": 1, \"replace\": 2, \"nested\": {\"x\": 1}}");

    TEST_ALLOCATOR source_allocator;
    TestAllocator_Init(&source_allocator);
    JSON_VALUE *const source = TestJson_Parse(
        &source_allocator.allocator,
        "{\"replace\": [\"a\", {\"b\": \"c\"}], \"nested\": {\"y\": \"z\"},"
        " \"new\": {\"list\": [1.5, \"\\u0000x\"]}}");
    CHECK_EQ_INT(source_allocator.live_blocks, 1);
    const size_t source_size = source_allocator.live_bytes;

    Json_Merge(target, source, M_Arena());
    CHECK_EQ_INT(source_allocator.live_blocks, 1);

    // Fills the source with garbage before releasing it, so that a read of it
    // through target shows even without a memory checker.
    memset(source, 0xA5, source_size);
    Allocator_Free(&source_allocator.allocator, source, source_size);
    CHECK_EQ_INT(source_allocator.live_blocks, 0);
    CHECK_EQ_INT(source_allocator.size_errors, 0);

    CHECK_EQ_STR(
        TestJson_Dump(target),
        "{\"keep\":1,\"replace\":[\"a\",{\"b\":\"c\"}],"
        "\"nested\":{\"x\":1,\"y\":\"z\"},\"new\":{\"list\":[1.5,\"\"]}}");
    const JSON_VALUE *const text =
        Json_At(Json_Get(Json_Get(target, "new"), "list"), 1);
    CHECK(text != nullptr && text->string.size == 2);
    CHECK(text != nullptr && memcmp(text->string.data, "\0x", 3) == 0);
}

TEST(merge_takes_its_blocks_from_the_allocator)
{
    JSON_VALUE *const target = TestJson_Parse(M_Arena(), "{\"a\": 1}");
    const JSON_VALUE *const source =
        TestJson_Parse(M_Arena(), "{\"a\": \"x\", \"b\": [true]}");
    TEST_ALLOCATOR merge_allocator;
    TestAllocator_Init(&merge_allocator);
    Json_Merge(target, source, &merge_allocator.allocator);
    // "x"; the grown members; the value of "b", its items and its key.
    CHECK_EQ_INT(merge_allocator.live_blocks, 5);
    CHECK_EQ_STR(TestJson_Dump(target), "{\"a\":\"x\",\"b\":[true]}");

    const JSON_VALUE *const b = Json_Get(target, "b");
    Allocator_Free(
        &merge_allocator.allocator,
        (void *)target->object.members[0].value->string.data, 2);
    Allocator_Free(
        &merge_allocator.allocator, b->array.items, sizeof(JSON_VALUE));
    Allocator_Free(
        &merge_allocator.allocator, (void *)target->object.members[1].key, 2);
    Allocator_Free(
        &merge_allocator.allocator, target->object.members[1].value,
        sizeof(JSON_VALUE));
    Allocator_Free(
        &merge_allocator.allocator, target->object.members,
        2 * sizeof(JSON_MEMBER));
    CHECK_EQ_INT(merge_allocator.live_blocks, 0);
    CHECK_EQ_INT(merge_allocator.size_errors, 0);
}

TEST(merge_handles_large_objects)
{
    // Builds a target with the keys 0 to 1999 and a source with the keys 1000
    // to 2999, with the source's values marked by a minus sign.
    TEST_JSON_TEXT target_text;
    TEST_JSON_TEXT source_text;
    Array_Init(&target_text, &g_HeapAllocator);
    Array_Init(&source_text, &g_HeapAllocator);
    TestJson_Append(&target_text, "{");
    TestJson_Append(&source_text, "{");
    for (int32_t i = 0; i < 2000; i++) {
        TestJson_Append(&target_text, "\"k%d\": %d,", i, i);
        TestJson_Append(&source_text, "\"k%d\": %d,", i + 1000, -(i + 1000));
    }
    TestJson_Append(&target_text, "}");
    TestJson_Append(&source_text, "\"k5\": {\"x\": 1}}");

    JSON_VALUE *const target = TestJson_Parse(M_Arena(), target_text.items);
    const JSON_VALUE *const source =
        TestJson_Parse(M_Arena(), source_text.items);
    Array_Free(&target_text);
    Array_Free(&source_text);
    Json_Merge(target, source, M_Arena());

    CHECK(target != nullptr && target->object.count == 3000);
    bool all_right = true;
    for (int32_t i = 0; i < 3000; i++) {
        char key[16];
        snprintf(key, sizeof(key), "k%d", i);
        all_right = all_right && target != nullptr
            && strcmp(target->object.members[i].key, key) == 0;
        if (i != 5) {
            const int64_t expected = i < 1000 ? i : -i;
            all_right =
                all_right && Json_GetInt(Json_Get(target, key), 0) == expected;
        }
    }
    CHECK(all_right);
    CHECK_EQ_STR(TestJson_Dump(Json_Get(target, "k5")), "{\"x\":1}");
}
