// SPDX-License-Identifier: GPL-3.0-only

#include "core/base/memory.h"

#include "test.h"
#include "test_allocator.h"

#include <stdint.h>

static bool M_IsZero(const void *const memory, const size_t size)
{
    const uint8_t *const bytes = memory;
    for (size_t i = 0; i < size; i++) {
        if (bytes[i] != 0) {
            return false;
        }
    }
    return true;
}

TEST(alloc_z_fills_with_zeros)
{
    uint8_t *const memory = Mem_AllocZ(256);
    CHECK(M_IsZero(memory, 256));
    Mem_Free(memory);

    int32_t *const values = Mem_AllocArrZ(100, sizeof(int32_t));
    CHECK(M_IsZero(values, 100 * sizeof(int32_t)));
    Mem_Free(values);
}

TEST(alloc_returns_writable_blocks)
{
    uint8_t *const memory = Mem_Alloc(64);
    memset(memory, 0xAB, 64);
    CHECK_EQ_INT(memory[63], 0xAB);
    Mem_Free(memory);

    int32_t *const values = Mem_AllocArr(10, sizeof(int32_t));
    values[9] = 9;
    CHECK_EQ_INT(values[9], 9);
    Mem_Free(values);
}

TEST(alloc_of_zero_bytes_returns_a_block)
{
    void *const memory = Mem_Alloc(0);
    CHECK(memory != nullptr);
    Mem_Free(memory);
}

TEST(realloc_keeps_the_contents)
{
    char *memory = Mem_Realloc(nullptr, 4);
    memcpy(memory, "abc", 4);
    memory = Mem_ReallocArr(memory, 4096, 2);
    CHECK_EQ_STR(memory, "abc");
    memory = Mem_Realloc(memory, 2);
    CHECK(memcmp(memory, "ab", 2) == 0);
    Mem_Free(memory);
}

TEST(dup_copies_the_data)
{
    const uint8_t data[] = { 1, 2, 3 };
    uint8_t *const copy = Mem_Dup(data, sizeof(data));
    CHECK(copy != data);
    CHECK(memcmp(copy, data, sizeof(data)) == 0);
    Mem_Free(copy);
}

TEST(free_of_nullptr_does_nothing)
{
    Mem_Free(nullptr);
    Allocator_Free(&g_HeapAllocator, nullptr, 0);
}

TEST(arr_size_multiplies)
{
    CHECK_EQ_INT(Mem_ArrSize(7, 3), 21);
    CHECK_EQ_INT(Mem_ArrSize(0, SIZE_MAX), 0);
}

TEST(heap_allocator_allocates_and_resizes)
{
    const ALLOCATOR *const heap = &g_HeapAllocator;
    char *memory = Allocator_AllocZ(heap, 16);
    CHECK(M_IsZero(memory, 16));
    memcpy(memory, "th06", 5);
    memory = Allocator_Realloc(heap, memory, 16, 1024);
    CHECK_EQ_STR(memory, "th06");
    Allocator_Free(heap, memory, 1024);
}

TEST(allocator_functions_pass_sizes_through)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    const ALLOCATOR *const allocator = &test.allocator;

    char *memory = Allocator_Alloc(allocator, 8);
    memcpy(memory, "abcdefg", 8);
    memory = Allocator_Realloc(allocator, memory, 8, 100);
    CHECK_EQ_STR(memory, "abcdefg");
    CHECK_EQ_INT(test.live_blocks, 1);
    CHECK_EQ_INT(test.live_bytes, 100);
    Allocator_Free(allocator, memory, 100);
    CHECK_EQ_INT(test.live_blocks, 0);
    CHECK_EQ_INT(test.size_errors, 0);
}
