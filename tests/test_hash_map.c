// SPDX-License-Identifier: GPL-3.0-only

#include "core/base/arena.h"
#include "core/base/hash_map.h"

#include "test.h"
#include "test_allocator.h"

typedef struct {
    int32_t font;
    int16_t size;
    uint8_t bold;
    uint8_t italic;
} FONT_KEY;

typedef struct {
    int32_t texture;
    float height;
} FONT;

typedef HASH_MAP(int32_t, int32_t) INT_MAP;
typedef HASH_MAP(FONT_KEY, FONT) FONT_CACHE;
typedef HASH_MAP(const char *, int32_t) NAME_MAP;

static int32_t M_GetInt(const INT_MAP *const map, const int32_t key)
{
    const int32_t *const value = HashMap_Get(map, key);
    return value == nullptr ? -1 : *value;
}

TEST(an_empty_map_contains_nothing)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    INT_MAP map;
    HashMap_Init(&map, &test.allocator);
    CHECK(HashMap_Get(&map, 1) == nullptr);
    CHECK(!HashMap_Contains(&map, 1));
    CHECK(!HashMap_Remove(&map, 1));
    int32_t index = 0;
    CHECK(!HashMap_Next(&map, &index));
    CHECK_EQ_INT(test.live_blocks, 0);
    HashMap_Free(&map);
}

TEST(put_then_get_through_growth)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    INT_MAP map;
    HashMap_Init(&map, &test.allocator);
    for (int32_t i = 0; i < 5000; i++) {
        HashMap_Put(&map, i, i * 3);
    }
    CHECK_EQ_INT(map.count, 5000);
    bool all_found = true;
    for (int32_t i = 0; i < 5000; i++) {
        all_found = all_found && M_GetInt(&map, i) == i * 3;
    }
    CHECK(all_found);
    CHECK_EQ_INT(M_GetInt(&map, 5000), -1);
    HashMap_Free(&map);
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.size_errors, 0);
}

TEST(put_replaces_the_value_of_a_key)
{
    INT_MAP map;
    HashMap_Init(&map, &g_HeapAllocator);
    HashMap_Put(&map, 7, 1);
    HashMap_Put(&map, 7, 2);
    CHECK_EQ_INT(map.count, 1);
    CHECK_EQ_INT(M_GetInt(&map, 7), 2);
    HashMap_Free(&map);
}

TEST(get_or_put_adds_a_zeroed_value_once)
{
    FONT_CACHE cache;
    HashMap_Init(&cache, &g_HeapAllocator);
    const FONT_KEY key = { .font = 1, .size = 16, .bold = 1 };

    bool found = true;
    FONT *const font = HashMap_GetOrPut(&cache, key, &found);
    CHECK(!found);
    CHECK_EQ_INT(font->texture, 0);
    font->texture = 42;

    FONT *const again = HashMap_GetOrPut(&cache, key, &found);
    CHECK(found);
    CHECK(again == font);
    CHECK_EQ_INT(HashMap_GetOrPut(&cache, key)->texture, 42);
    CHECK_EQ_INT(cache.count, 1);
    HashMap_Free(&cache);
}

TEST(struct_keys_compare_every_field)
{
    FONT_CACHE cache;
    HashMap_Init(&cache, &g_HeapAllocator);
    FONT_KEY key = { .font = 1, .size = 16 };
    HashMap_Put(&cache, key, ((FONT) { .texture = 100 }));
    key.bold = 1;
    HashMap_Put(&cache, key, ((FONT) { .texture = 200 }));

    FONT_KEY lookup = { .font = 1, .size = 16 };
    CHECK_EQ_INT(HashMap_Get(&cache, lookup)->texture, 100);
    lookup.bold = 1;
    CHECK_EQ_INT(HashMap_Get(&cache, lookup)->texture, 200);
    lookup.size = 15;
    CHECK(HashMap_Get(&cache, lookup) == nullptr);
    HashMap_Free(&cache);
}

TEST(string_keys_are_copied)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    NAME_MAP map;
    HashMap_Init(&map, &test.allocator);
    char name[] = "title.anm";
    HashMap_Put(&map, name, 1);
    HashMap_Put(&map, "東方紅魔郷", 6);
    name[0] = 'x';
    CHECK(HashMap_Get(&map, name) == nullptr);
    CHECK_EQ_INT(*HashMap_Get(&map, "title.anm"), 1);
    CHECK_EQ_INT(*HashMap_Get(&map, "東方紅魔郷"), 6);
    CHECK(!HashMap_Contains(&map, "title"));
    CHECK(HashMap_Remove(&map, "title.anm"));
    CHECK(!HashMap_Contains(&map, "title.anm"));
    CHECK_EQ_INT(map.count, 1);
    HashMap_Free(&map);
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.size_errors, 0);
}

TEST(remove_keeps_every_other_key_reachable)
{
    INT_MAP map;
    HashMap_Init(&map, &g_HeapAllocator);
    for (int32_t i = 0; i < 2000; i++) {
        HashMap_Put(&map, i, i);
    }
    for (int32_t i = 0; i < 2000; i += 3) {
        CHECK(HashMap_Remove(&map, i));
    }
    CHECK(!HashMap_Remove(&map, 0));

    bool all_right = true;
    int32_t expected_count = 0;
    for (int32_t i = 0; i < 2000; i++) {
        const int32_t expected = i % 3 == 0 ? -1 : i;
        all_right = all_right && M_GetInt(&map, i) == expected;
        expected_count += expected >= 0 ? 1 : 0;
    }
    CHECK(all_right);
    CHECK_EQ_INT(map.count, expected_count);

    for (int32_t i = 0; i < 2000; i++) {
        HashMap_Remove(&map, i);
    }
    CHECK_EQ_INT(map.count, 0);
    HashMap_Free(&map);
}

TEST(iteration_visits_every_entry_once)
{
    NAME_MAP map;
    HashMap_Init(&map, &g_HeapAllocator);
    const char *const names[] = { "a", "bb", "ccc", "dddd", "eeeee" };
    for (int32_t i = 0; i < 5; i++) {
        HashMap_Put(&map, names[i], i);
    }

    int32_t seen = 0;
    int32_t visits = 0;
    for (int32_t i = 0; HashMap_Next(&map, &i); i++) {
        CHECK_EQ_STR(map.keys[i], names[map.values[i]]);
        seen |= 1 << map.values[i];
        visits++;
    }
    CHECK_EQ_INT(visits, 5);
    CHECK_EQ_INT(seen, 0x1F);
    HashMap_Free(&map);
}

TEST(clear_empties_the_map_for_reuse)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    NAME_MAP map;
    HashMap_Init(&map, &test.allocator);
    HashMap_Put(&map, "a", 1);
    HashMap_Put(&map, "b", 2);
    HashMap_Clear(&map);
    CHECK_EQ_INT(map.count, 0);
    CHECK(!HashMap_Contains(&map, "a"));
    CHECK_EQ_INT(test.live_blocks, 3);
    HashMap_Put(&map, "a", 3);
    CHECK_EQ_INT(*HashMap_Get(&map, "a"), 3);
    HashMap_Free(&map);
    HashMap_Put(&map, "c", 4);
    CHECK_EQ_INT(map.count, 1);
    HashMap_Free(&map);
    CHECK_EQ_INT(test.live_blocks, 0);
}

TEST(a_map_in_an_arena_survives_a_restore)
{
    ARENA arena;
    Arena_Init(&arena, "sim", 1 << 20);
    NAME_MAP *const map = Arena_Alloc(&arena, sizeof(NAME_MAP));
    HashMap_Init(map, Arena_Allocator(&arena));
    HashMap_Put(map, "reimu", 1);
    ARENA_SNAPSHOT snapshot = {};
    Arena_Save(&arena, &snapshot);

    for (int32_t i = 0; i < 100; i++) {
        char name[16];
        snprintf(name, sizeof(name), "fairy%d", i);
        HashMap_Put(map, name, i);
    }
    HashMap_Remove(map, "reimu");

    Arena_Restore(&arena, &snapshot);
    CHECK_EQ_INT(map->count, 1);
    CHECK_EQ_INT(*HashMap_Get(map, "reimu"), 1);
    CHECK(!HashMap_Contains(map, "fairy0"));
    Arena_FreeSnapshot(&snapshot);
    Arena_Free(&arena);
}
