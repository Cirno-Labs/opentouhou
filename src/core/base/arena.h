// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/memory.h"
#include "core/base/types.h"

// Allocates blocks from one region of memory that is reserved up front, and
// releases them all at once.
//
// The region has a fixed capacity and a fixed address for the whole life of
// the arena. It never grows or moves, and an allocation that does not fit is
// fatal. Because of the fixed address, a snapshot is a plain copy of the
// allocated bytes: after Arena_Restore, every pointer from one block of the
// arena to another is valid again. Pointers out of the arena are copied
// unchanged, so they stay valid only if their targets outlive the snapshot.
//
// Pages of the region that no block has reached are never touched, so a
// large capacity costs address space but no physical memory.

typedef struct {
    const char *name;
    unsigned char *base;
    size_t capacity;
    size_t used;
    ALLOCATOR allocator;
} ARENA;

// Holds a copy of the allocated part of one arena. A zero-initialized
// snapshot is empty; Arena_Save fills it and reuses its buffer when saved
// to again.
typedef struct {
    const unsigned char *arena_base;
    unsigned char *data;
    size_t size;
    size_t capacity;
} ARENA_SNAPSHOT;

// Reserves capacity bytes. The fatal error for a full arena gives its name,
// which must stay valid for the life of the arena. The ARENA itself must not
// move while its allocator is in use.
void Arena_Init(ARENA *arena, const char *name, size_t capacity);

// Releases the region. Every block and every snapshot of the arena becomes
// invalid.
void Arena_Free(ARENA *arena);

// Returns size bytes aligned for any type. A block that does not fit in the
// free part of the region is fatal. Arena_Alloc leaves the block
// uninitialized; Arena_AllocZ fills it with zeros.
void *Arena_Alloc(ARENA *arena, size_t size);
void *Arena_AllocZ(ARENA *arena, size_t size);

// Return count elements of size bytes each, as Arena_Alloc and Arena_AllocZ
// do. A total size that overflows size_t is fatal.
void *Arena_AllocArr(ARENA *arena, size_t count, size_t size);
void *Arena_AllocArrZ(ARENA *arena, size_t count, size_t size);

// Returns an allocator that takes its blocks from the arena, so that
// containers can live in it. Its realloc allocates a new block and copies
// the contents into it, and its free does nothing: the memory comes back
// when the arena is reset or restored. A full arena is fatal.
const ALLOCATOR *Arena_Allocator(ARENA *arena);

// Releases every block. The region stays reserved and the next block starts
// at its beginning.
void Arena_Reset(ARENA *arena);

// Copies the allocated blocks of the arena into a snapshot.
void Arena_Save(const ARENA *arena, ARENA_SNAPSHOT *snapshot);

// Returns the arena to its state when the snapshot was saved. The blocks
// allocated before Arena_Save get back their contents at the same addresses;
// the blocks allocated after it are released. A snapshot of another arena is
// fatal.
void Arena_Restore(ARENA *arena, const ARENA_SNAPSHOT *snapshot);

// Releases the buffer of a snapshot and empties it.
void Arena_FreeSnapshot(ARENA_SNAPSHOT *snapshot);
