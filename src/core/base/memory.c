#include "core/base/memory.h"

#include "core/base/log.h"

#include <stdckdint.h>
#include <stdlib.h>
#include <string.h>

static void *M_HeapAlloc(void *const context, const size_t size)
{
    return malloc(size == 0 ? 1 : size);
}

static void *M_HeapRealloc(
    void *const context, void *const memory, const size_t old_size,
    const size_t new_size)
{
    return realloc(memory, new_size == 0 ? 1 : new_size);
}

static void M_HeapFree(
    void *const context, void *const memory, const size_t size)
{
    free(memory);
}

const ALLOCATOR g_HeapAllocator = {
    .alloc = M_HeapAlloc,
    .realloc = M_HeapRealloc,
    .free = M_HeapFree,
};

size_t Mem_ArrSize(const size_t count, const size_t size)
{
    size_t total;
    if (ckd_mul(&total, count, size)) {
        LOG_FATAL(
            "Array of %zu elements of %zu bytes is too large", count, size);
    }
    return total;
}

void *Mem_Alloc(const size_t size)
{
    return Allocator_Alloc(&g_HeapAllocator, size);
}

void *Mem_AllocZ(const size_t size)
{
    void *const memory = calloc(1, size == 0 ? 1 : size);
    if (memory == nullptr) {
        LOG_FATAL("Out of memory allocating %zu bytes", size);
    }
    return memory;
}

void *Mem_AllocArr(const size_t count, const size_t size)
{
    return Mem_Alloc(Mem_ArrSize(count, size));
}

void *Mem_AllocArrZ(const size_t count, const size_t size)
{
    return Mem_AllocZ(Mem_ArrSize(count, size));
}

void *Mem_Realloc(void *const memory, const size_t size)
{
    return Allocator_Realloc(&g_HeapAllocator, memory, 0, size);
}

void *Mem_ReallocArr(void *const memory, const size_t count, const size_t size)
{
    return Mem_Realloc(memory, Mem_ArrSize(count, size));
}

void Mem_Free(void *const memory)
{
    free(memory);
}

void *Mem_Dup(const void *const data, const size_t size)
{
    void *const copy = Mem_Alloc(size);
    if (size != 0) {
        memcpy(copy, data, size);
    }
    return copy;
}

void *Allocator_Alloc(const ALLOCATOR *const allocator, const size_t size)
{
    void *const memory = allocator->alloc(allocator->context, size);
    if (memory == nullptr) {
        LOG_FATAL("Out of memory allocating %zu bytes", size);
    }
    return memory;
}

void *Allocator_AllocZ(const ALLOCATOR *const allocator, const size_t size)
{
    void *const memory = Allocator_Alloc(allocator, size);
    memset(memory, 0, size);
    return memory;
}

void *Allocator_Realloc(
    const ALLOCATOR *const allocator, void *const memory, const size_t old_size,
    const size_t new_size)
{
    void *const result =
        allocator->realloc(allocator->context, memory, old_size, new_size);
    if (result == nullptr) {
        LOG_FATAL("Out of memory allocating %zu bytes", new_size);
    }
    return result;
}

void Allocator_Free(
    const ALLOCATOR *const allocator, void *const memory, const size_t size)
{
    if (memory != nullptr) {
        allocator->free(allocator->context, memory, size);
    }
}
