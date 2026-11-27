#include "tests/test_allocator.h"

#include <stdlib.h>
#include <string.h>

static void *M_Allocate(TEST_ALLOCATOR *const test, const size_t size)
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

static void M_Release(
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

static void *M_Alloc(void *const context, const size_t size)
{
    return M_Allocate(context, size);
}

static void *M_Realloc(
    void *const context, void *const memory, const size_t old_size,
    const size_t new_size)
{
    void *const result = M_Allocate(context, new_size);
    if (result != nullptr && memory != nullptr) {
        memcpy(result, memory, old_size < new_size ? old_size : new_size);
        M_Release(context, memory, old_size);
    }
    return result;
}

static void M_Free(void *const context, void *const memory, const size_t size)
{
    M_Release(context, memory, size);
}

void TestAllocator_Init(TEST_ALLOCATOR *const test)
{
    *test = (TEST_ALLOCATOR) {
        .allocator = {
            .alloc = M_Alloc,
            .realloc = M_Realloc,
            .free = M_Free,
            .context = test,
        },
        .allocations_left = -1,
    };
}
