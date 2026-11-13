#pragma once

#include "core/base/types.h"

// Allocates from the C heap. Running out of memory is fatal, so these
// functions never return nullptr. Mem_Free releases what they return. The
// functions without Z leave the new memory uninitialized; the Z ones fill
// it with zeros.

// Returns size bytes. A size of 0 returns a valid block that must not be
// accessed.
void *Mem_Alloc(size_t size);
void *Mem_AllocZ(size_t size);

// Returns count elements of size bytes each. A total size that overflows
// size_t is fatal.
void *Mem_AllocArr(size_t count, size_t size);
void *Mem_AllocArrZ(size_t count, size_t size);

// Resizes a block to size bytes, which may move it. The contents up to the
// smaller of the two sizes are kept; bytes beyond the old size are
// uninitialized. A memory of nullptr allocates a new block.
void *Mem_Realloc(void *memory, size_t size);

// Resizes a block to count elements of size bytes each, as Mem_Realloc
// does. A total size that overflows size_t is fatal.
void *Mem_ReallocArr(void *memory, size_t count, size_t size);

// Releases a block. A memory of nullptr does nothing.
void Mem_Free(void *memory);

// Returns a copy of size bytes of data.
void *Mem_Dup(const void *data, size_t size);

// Returns count * size, or ends the program if the product overflows
// size_t.
size_t Mem_ArrSize(size_t count, size_t size);

// Provides memory to the containers and to other code that does not decide
// where its memory comes from. The functions receive context as their first
// argument. alloc and realloc return nullptr when they cannot provide the
// memory; the Allocator_ functions make that fatal. Blocks are aligned for
// any type. realloc with a memory of nullptr allocates a new block. realloc
// and free receive the size the block has, so that an allocator does not
// have to record it.
//
// g_HeapAllocator takes its memory from the C heap, and Arena_Allocator
// returns one that takes it from an arena.
typedef struct {
    void *(*alloc)(void *context, size_t size);
    void *(*realloc)(
        void *context, void *memory, size_t old_size, size_t new_size);
    void (*free)(void *context, void *memory, size_t size);
    void *context;
} ALLOCATOR;

extern const ALLOCATOR g_HeapAllocator;

// Call the functions of an allocator, as Mem_Alloc, Mem_AllocZ, Mem_Realloc
// and Mem_Free do for the heap. An allocator that returns nullptr is fatal.
void *Allocator_Alloc(const ALLOCATOR *allocator, size_t size);
void *Allocator_AllocZ(const ALLOCATOR *allocator, size_t size);
void *Allocator_Realloc(
    const ALLOCATOR *allocator, void *memory, size_t old_size, size_t new_size);
void Allocator_Free(const ALLOCATOR *allocator, void *memory, size_t size);
