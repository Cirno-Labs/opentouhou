#pragma once

#include "core/base/memory.h"
#include "core/base/types.h"

// Holds a growable array of elements of one type, with its memory from an
// ALLOCATOR. ARRAY(type) declares the struct. Every use of ARRAY(type) is a
// distinct type, so each element type gets one typedef.
//
// Example:
// typedef ARRAY(SPRITE) SPRITE_ARRAY;
// SPRITE_ARRAY sprites;
// Array_Init(&sprites, &g_HeapAllocator);
// Array_Append(&sprites, sprite);
// SPRITE *const first = &sprites.items[0];
// Array_Free(&sprites);
//
// items points to count elements, with room for capacity. Adding elements
// may move them, which makes pointers to elements invalid. An array whose
// allocator comes from Arena_Allocator can be part of the simulation state.
//
// The Array_ macros work on every ARRAY(type). They take the element type
// from the array, so an item of another type fails to compile, and they
// evaluate each argument once. They call the one generic implementation,
// the ArrayImpl_ functions, which are not meant to be called directly.
#define ARRAY(type_)                                                           \
    struct {                                                                   \
        type_ *items;                                                          \
        int32_t count;                                                         \
        int32_t capacity;                                                      \
        const ALLOCATOR *allocator;                                            \
    }

// Prepares an empty array. It allocates nothing until the first element is
// added.
#define Array_Init(array_, allocator_)                                         \
    ArrayImpl_Init(ARRAY_IMPL_OF(array_), (allocator_))

// Releases the storage and leaves the array empty, ready for reuse with the
// same allocator.
#define Array_Free(array_)                                                     \
    ArrayImpl_Free(ARRAY_IMPL_OF(array_), ARRAY_ITEM_SIZE(array_))

// Grows the storage to hold at least capacity elements.
#define Array_EnsureCapacity(array_, capacity_)                                \
    ArrayImpl_EnsureCapacity(                                                  \
        ARRAY_IMPL_OF(array_), ARRAY_ITEM_SIZE(array_), (capacity_))

// Returns a pointer to the element at index. An index out of range is fatal.
#define Array_Get(array_, index_)                                              \
    ((typeof((array_)->items))ArrayImpl_Get(                                   \
        ARRAY_IMPL_OF(array_), ARRAY_ITEM_SIZE(array_), (index_)))

// Adds an uninitialized element at the end and returns a pointer to it.
#define Array_AddOne(array_)                                                   \
    ((typeof((array_)->items))ArrayImpl_AddOne(                                \
        ARRAY_IMPL_OF(array_), ARRAY_ITEM_SIZE(array_)))

// Adds a copy of item at the end.
#define Array_Append(array_, item_) ((void)(*Array_AddOne(array_) = (item_)))

// Adds copies of count elements at the end. items must point to elements of
// the array's type, outside the array.
#define Array_AppendSlice(array_, items_, count_)                              \
    ArrayImpl_AppendSlice(                                                     \
        ARRAY_IMPL_OF(array_), ARRAY_ITEM_SIZE(array_),                        \
        ARRAY_CHECK_ITEMS(array_, items_), (count_))

// Inserts a copy of item at index, moving the elements from index onwards up
// by one. An index equal to the count appends.
#define Array_Insert(array_, index_, item_)                                    \
    ((void)(*(typeof((array_)->items))ArrayImpl_InsertOne(                     \
                ARRAY_IMPL_OF(array_), ARRAY_ITEM_SIZE(array_), (index_)) =    \
                (item_)))

// Removes the last element and returns it. An empty array is fatal.
#define Array_Pop(array_)                                                      \
    (*(typeof((array_)->items))ArrayImpl_Pop(                                  \
        ARRAY_IMPL_OF(array_), ARRAY_ITEM_SIZE(array_)))

// Removes the element at index and moves the later elements down by one, so
// the order stays the same.
#define Array_OrderedRemove(array_, index_)                                    \
    ArrayImpl_OrderedRemove(                                                   \
        ARRAY_IMPL_OF(array_), ARRAY_ITEM_SIZE(array_), (index_))

// Removes the element at index and moves the last element into its place.
// This takes constant time but changes the order.
#define Array_SwapRemove(array_, index_)                                       \
    ArrayImpl_SwapRemove(                                                      \
        ARRAY_IMPL_OF(array_), ARRAY_ITEM_SIZE(array_), (index_))

// Removes every element and keeps the storage.
#define Array_Clear(array_) ((void)((array_)->count = 0))

// The rest of this header is the generic implementation behind the macros.

typedef ARRAY(void) ARRAY_IMPL;

#define ARRAY_IMPL_OF(array_) ((ARRAY_IMPL *)(array_))
#define ARRAY_ITEM_SIZE(array_) sizeof(*(array_)->items)

// Evaluates to items if it points to the element type of the array, and
// fails to compile otherwise.
#define ARRAY_CHECK_ITEMS(array_, items_)                                      \
    _Generic(                                                                  \
        (items_),                                                              \
        typeof((array_)->items): (items_),                                     \
        const typeof(*(array_)->items) *: (items_))

void ArrayImpl_Init(ARRAY_IMPL *array, const ALLOCATOR *allocator);
void ArrayImpl_Free(ARRAY_IMPL *array, size_t item_size);
void ArrayImpl_EnsureCapacity(
    ARRAY_IMPL *array, size_t item_size, int32_t capacity);
void *ArrayImpl_Get(const ARRAY_IMPL *array, size_t item_size, int32_t index);
void *ArrayImpl_AddOne(ARRAY_IMPL *array, size_t item_size);
void ArrayImpl_AppendSlice(
    ARRAY_IMPL *array, size_t item_size, const void *items, int32_t count);
void *ArrayImpl_InsertOne(ARRAY_IMPL *array, size_t item_size, int32_t index);
void *ArrayImpl_Pop(ARRAY_IMPL *array, size_t item_size);
void ArrayImpl_OrderedRemove(
    ARRAY_IMPL *array, size_t item_size, int32_t index);
void ArrayImpl_SwapRemove(ARRAY_IMPL *array, size_t item_size, int32_t index);
