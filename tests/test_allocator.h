#pragma once

// Provides an allocator for tests that counts its live blocks, checks the
// sizes that realloc and free receive, and fails on request.

#include "core/base/memory.h"

typedef struct {
    ALLOCATOR allocator;
    int32_t live_blocks;
    size_t live_bytes;
    int32_t size_errors;
    // Sets how many more allocations succeed. Negative means all of them.
    int32_t allocations_left;
} TEST_ALLOCATOR;

void TestAllocator_Init(TEST_ALLOCATOR *test);
