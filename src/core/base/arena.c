#include "core/base/arena.h"

#include "core/base/assert.h"
#include "core/base/log.h"

#include <string.h>

#define M_ALIGNMENT alignof(max_align_t)

static void *M_AllocatorAlloc(void *const context, const size_t size)
{
    return Arena_Alloc(context, size);
}

static void *M_AllocatorRealloc(
    void *const context, void *const memory, const size_t old_size,
    const size_t new_size)
{
    if (memory != nullptr && new_size <= old_size) {
        return memory;
    }
    void *const block = Arena_Alloc(context, new_size);
    if (memory != nullptr) {
        memcpy(block, memory, old_size);
    }
    return block;
}

static void M_AllocatorFree(
    void *const context, void *const memory, const size_t size)
{
}

void Arena_Init(
    ARENA *const arena, const char *const name, const size_t capacity)
{
    // Mem_Alloc leaves the block uninitialized, so the pages of the region
    // are not touched until a block reaches them.
    *arena = (ARENA) {
        .name = name,
        .base = Mem_Alloc(capacity),
        .capacity = capacity,
        .allocator = {
            .alloc = M_AllocatorAlloc,
            .realloc = M_AllocatorRealloc,
            .free = M_AllocatorFree,
            .context = arena,
        },
    };
}

void Arena_Free(ARENA *const arena)
{
    Mem_Free(arena->base);
    *arena = (ARENA) {};
}

void *Arena_Alloc(ARENA *const arena, const size_t size)
{
    const size_t start = (arena->used + M_ALIGNMENT - 1) & ~(M_ALIGNMENT - 1);
    if (start > arena->capacity || size > arena->capacity - start) {
        LOG_FATAL(
            "Arena %s is full: %zu bytes requested, %zu of %zu in use",
            arena->name, size, arena->used, arena->capacity);
    }
    arena->used = start + size;
    return arena->base + start;
}

void *Arena_AllocZ(ARENA *const arena, const size_t size)
{
    void *const block = Arena_Alloc(arena, size);
    memset(block, 0, size);
    return block;
}

void *Arena_AllocArr(ARENA *const arena, const size_t count, const size_t size)
{
    return Arena_Alloc(arena, Mem_ArrSize(count, size));
}

void *Arena_AllocArrZ(ARENA *const arena, const size_t count, const size_t size)
{
    return Arena_AllocZ(arena, Mem_ArrSize(count, size));
}

const ALLOCATOR *Arena_Allocator(ARENA *const arena)
{
    ASSERT(arena->allocator.context == arena);
    return &arena->allocator;
}

void Arena_Reset(ARENA *const arena)
{
    arena->used = 0;
}

void Arena_Save(const ARENA *const arena, ARENA_SNAPSHOT *const snapshot)
{
    if (snapshot->capacity < arena->used) {
        snapshot->data = Mem_Realloc(snapshot->data, arena->used);
        snapshot->capacity = arena->used;
    }
    if (arena->used != 0) {
        memcpy(snapshot->data, arena->base, arena->used);
    }
    snapshot->arena_base = arena->base;
    snapshot->size = arena->used;
}

void Arena_Restore(ARENA *const arena, const ARENA_SNAPSHOT *const snapshot)
{
    ASSERT(snapshot->arena_base == arena->base);
    if (snapshot->size != 0) {
        memcpy(arena->base, snapshot->data, snapshot->size);
    }
    arena->used = snapshot->size;
}

void Arena_FreeSnapshot(ARENA_SNAPSHOT *const snapshot)
{
    Mem_Free(snapshot->data);
    *snapshot = (ARENA_SNAPSHOT) {};
}
