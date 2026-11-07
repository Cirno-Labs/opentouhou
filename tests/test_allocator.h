#pragma once

// Provides an allocator for tests that counts its live blocks, checks the
// sizes that realloc and free receive, and fails on request.

#include "core/base/memory.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    ALLOCATOR allocator;
    int32_t live_blocks;
    size_t live_bytes;
    int32_t size_errors;
    // Sets how many more allocations succeed. Negative means all of them.
    int32_t allocations_left;
} TEST_ALLOCATOR;

static void *M_TestAllocate(TEST_ALLOCATOR *const test, const size_t size)
{
    if (test->allocations_left == 0) {
        return nullptr;
    }
    if (test->allocations_left > 0) {
        test->allocations_left--;
    }
    max_align_t *const header = malloc(sizeof(max_align_t) + size);
    memcpy(header, &size, sizeof(size));
    test->live_blocks++;
    test->live_bytes += size;
    return header + 1;
}

static void M_TestRelease(
    TEST_ALLOCATOR *const test, void *const memory, const size_t size)
{
    max_align_t *const header = (max_align_t *)memory - 1;
    size_t recorded;
    memcpy(&recorded, header, sizeof(recorded));
    if (recorded != size) {
        test->size_errors++;
    }
    test->live_blocks--;
    test->live_bytes -= recorded;
    free(header);
}

static void *M_TestAlloc(void *const context, const size_t size)
{
    return M_TestAllocate(context, size);
}

static void *M_TestRealloc(
    void *const context, void *const memory, const size_t old_size,
    const size_t new_size)
{
    void *const result = M_TestAllocate(context, new_size);
    if (result != nullptr && memory != nullptr) {
        memcpy(result, memory, old_size < new_size ? old_size : new_size);
        M_TestRelease(context, memory, old_size);
    }
    return result;
}

static void M_TestFree(
    void *const context, void *const memory, const size_t size)
{
    M_TestRelease(context, memory, size);
}

static void TestAllocator_Init(TEST_ALLOCATOR *const test)
{
    *test = (TEST_ALLOCATOR) {
        .allocator = {
            .alloc = M_TestAlloc,
            .realloc = M_TestRealloc,
            .free = M_TestFree,
            .context = test,
        },
        .allocations_left = -1,
    };
}
