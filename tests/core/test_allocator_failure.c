#include "core/base/memory.h"

#include "test_allocator.h"

int main(void)
{
    TEST_ALLOCATOR test;
    TestAllocator_Init(&test);
    test.allocations_left = 1;
    void *const first = Allocator_Alloc(&test.allocator, 8);
    Allocator_Free(&test.allocator, Allocator_Alloc(&test.allocator, 8), 8);
    Allocator_Free(&test.allocator, first, 8);
    return EXIT_SUCCESS;
}
