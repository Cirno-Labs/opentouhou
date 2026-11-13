#include "core/base/arena.h"
#include "core/base/array.h"

#include "test.h"
#include "test_allocator.h"

typedef struct {
    float x;
    float y;
    int32_t id;
} SPRITE;

typedef ARRAY(int32_t) INT_ARRAY;
typedef ARRAY(SPRITE) SPRITE_ARRAY;

static INT_ARRAY M_Of(const int32_t *const values, const int32_t count)
{
    INT_ARRAY array;
    Array_Init(&array, &g_HeapAllocator);
    Array_AppendSlice(&array, values, count);
    return array;
}

static bool M_Equals(
    const INT_ARRAY *const array, const int32_t *const values,
    const int32_t count)
{
    return array->count == count
        && memcmp(array->items, values, (size_t)count * sizeof(int32_t)) == 0;
}

TEST(a_new_array_is_empty_and_allocates_nothing)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    INT_ARRAY array;
    Array_Init(&array, &test.allocator);
    CHECK_EQ_INT(array.count, 0);
    CHECK(array.items == nullptr);
    CHECK_EQ_INT(test.live_blocks, 0);
    Array_Free(&array);
}

TEST(append_keeps_order_through_growth)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    INT_ARRAY array;
    Array_Init(&array, &test.allocator);
    for (int32_t i = 0; i < 1000; i++) {
        Array_Append(&array, i);
    }
    CHECK_EQ_INT(array.count, 1000);
    CHECK(array.capacity >= 1000);
    bool in_order = true;
    for (int32_t i = 0; i < 1000; i++) {
        in_order = in_order && array.items[i] == i;
    }
    CHECK(in_order);
    CHECK_EQ_INT(test.live_blocks, 1);
    Array_Free(&array);
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.size_errors, 0);
}

TEST(struct_items_are_typed)
{
    SPRITE_ARRAY sprites;
    Array_Init(&sprites, &g_HeapAllocator);
    Array_Append(&sprites, ((SPRITE) { .x = 1.5f, .id = 7 }));
    SPRITE *const added = Array_AddOne(&sprites);
    *added = (SPRITE) { .y = 2.0f, .id = 8 };
    SPRITE *const first = &sprites.items[0];
    CHECK(first->x == 1.5f);
    CHECK_EQ_INT(Array_Get(&sprites, 1)->id, 8);
    Array_Free(&sprites);
}

TEST(append_slice_copies_every_element)
{
    INT_ARRAY array = M_Of((const int32_t[]) { 1, 2 }, 2);
    const int32_t more[] = { 3, 4, 5, 6, 7 };
    Array_AppendSlice(&array, more, 5);
    Array_AppendSlice(&array, more, 0);
    CHECK(M_Equals(&array, (const int32_t[]) { 1, 2, 3, 4, 5, 6, 7 }, 7));
    Array_Free(&array);
}

TEST(insert_moves_later_elements_up)
{
    INT_ARRAY array = M_Of((const int32_t[]) { 10, 30 }, 2);
    Array_Insert(&array, 1, 20);
    Array_Insert(&array, 0, 0);
    Array_Insert(&array, array.count, 40);
    CHECK(M_Equals(&array, (const int32_t[]) { 0, 10, 20, 30, 40 }, 5));
    Array_Free(&array);
}

TEST(pop_returns_the_last_element)
{
    INT_ARRAY array = M_Of((const int32_t[]) { 1, 2, 3 }, 3);
    CHECK_EQ_INT(Array_Pop(&array), 3);
    CHECK_EQ_INT(Array_Pop(&array), 2);
    CHECK_EQ_INT(array.count, 1);
    Array_Free(&array);
}

TEST(ordered_remove_keeps_the_order)
{
    INT_ARRAY array = M_Of((const int32_t[]) { 0, 1, 2, 3 }, 4);
    Array_OrderedRemove(&array, 1);
    CHECK(M_Equals(&array, (const int32_t[]) { 0, 2, 3 }, 3));
    Array_OrderedRemove(&array, 2);
    Array_OrderedRemove(&array, 0);
    CHECK(M_Equals(&array, (const int32_t[]) { 2 }, 1));
    Array_Free(&array);
}

TEST(swap_remove_moves_the_last_element_in)
{
    INT_ARRAY array = M_Of((const int32_t[]) { 0, 1, 2, 3 }, 4);
    Array_SwapRemove(&array, 1);
    CHECK(M_Equals(&array, (const int32_t[]) { 0, 3, 2 }, 3));
    Array_SwapRemove(&array, 2);
    CHECK(M_Equals(&array, (const int32_t[]) { 0, 3 }, 2));
    Array_Free(&array);
}

TEST(clear_keeps_the_storage)
{
    INT_ARRAY array = M_Of((const int32_t[]) { 1, 2, 3 }, 3);
    int32_t *const items = array.items;
    const int32_t capacity = array.capacity;
    Array_Clear(&array);
    CHECK_EQ_INT(array.count, 0);
    CHECK(array.items == items);
    CHECK_EQ_INT(array.capacity, capacity);
    Array_Free(&array);
}

TEST(ensure_capacity_grows_only)
{
    INT_ARRAY array;
    Array_Init(&array, &g_HeapAllocator);
    Array_EnsureCapacity(&array, 100);
    CHECK_EQ_INT(array.capacity, 100);
    Array_EnsureCapacity(&array, 10);
    CHECK_EQ_INT(array.capacity, 100);
    Array_Free(&array);
}

TEST(free_leaves_a_reusable_array)
{
    INT_ARRAY array = M_Of((const int32_t[]) { 1, 2 }, 2);
    Array_Free(&array);
    CHECK_EQ_INT(array.count, 0);
    CHECK(array.items == nullptr);
    Array_Append(&array, 7);
    CHECK_EQ_INT(array.items[0], 7);
    Array_Free(&array);
}

TEST(macro_arguments_are_evaluated_once)
{
    INT_ARRAY arrays[2];
    Array_Init(&arrays[0], &g_HeapAllocator);
    Array_Init(&arrays[1], &g_HeapAllocator);
    int32_t index = 0;
    Array_Append(&arrays[index++], 5);
    CHECK_EQ_INT(index, 1);
    CHECK_EQ_INT(arrays[0].count, 1);
    CHECK_EQ_INT(Array_Pop(&arrays[--index]), 5);
    CHECK_EQ_INT(index, 0);
    Array_Free(&arrays[0]);
    Array_Free(&arrays[1]);
}

TEST(an_array_in_an_arena_survives_a_restore)
{
    ARENA arena;
    Arena_Init(&arena, "sim", 1 << 16);
    INT_ARRAY *const array = Arena_Alloc(&arena, sizeof(INT_ARRAY));
    Array_Init(array, Arena_Allocator(&arena));
    for (int32_t i = 0; i < 10; i++) {
        Array_Append(array, i);
    }
    ARENA_SNAPSHOT snapshot = {};
    Arena_Save(&arena, &snapshot);

    for (int32_t i = 0; i < 100; i++) {
        Array_Append(array, -1);
    }
    array->items[0] = 99;

    Arena_Restore(&arena, &snapshot);
    CHECK_EQ_INT(array->count, 10);
    bool in_order = true;
    for (int32_t i = 0; i < 10; i++) {
        in_order = in_order && array->items[i] == i;
    }
    CHECK(in_order);
    Arena_FreeSnapshot(&snapshot);
    Arena_Free(&arena);
}
