// SPDX-License-Identifier: GPL-3.0-only

#include "core/base/array.h"

#include "core/base/assert.h"

#include <string.h>

#define M_MIN_CAPACITY 4

static char *M_At(
    const ARRAY_IMPL *const array, const size_t item_size, const int32_t index)
{
    return (char *)array->items + (size_t)index * item_size;
}

static void M_Grow(
    ARRAY_IMPL *const array, const size_t item_size, const int32_t extra)
{
    ASSERT(extra >= 0 && array->count <= INT32_MAX - extra);
    const int32_t needed = array->count + extra;
    if (needed <= array->capacity) {
        return;
    }
    int32_t capacity =
        array->capacity < M_MIN_CAPACITY ? M_MIN_CAPACITY : array->capacity;
    while (capacity < needed) {
        capacity = capacity <= INT32_MAX / 2 ? capacity * 2 : INT32_MAX;
    }
    ArrayImpl_EnsureCapacity(array, item_size, capacity);
}

void ArrayImpl_Init(ARRAY_IMPL *const array, const ALLOCATOR *const allocator)
{
    *array = (ARRAY_IMPL) { .allocator = allocator };
}

void ArrayImpl_Free(ARRAY_IMPL *const array, const size_t item_size)
{
    Allocator_Free(
        array->allocator, array->items, (size_t)array->capacity * item_size);
    *array = (ARRAY_IMPL) { .allocator = array->allocator };
}

void ArrayImpl_EnsureCapacity(
    ARRAY_IMPL *const array, const size_t item_size, const int32_t capacity)
{
    if (capacity <= array->capacity) {
        return;
    }
    array->items = Allocator_Realloc(
        array->allocator, array->items, (size_t)array->capacity * item_size,
        Mem_ArrSize((size_t)capacity, item_size));
    array->capacity = capacity;
}

void *ArrayImpl_Get(
    const ARRAY_IMPL *const array, const size_t item_size, const int32_t index)
{
    ASSERT(index >= 0 && index < array->count);
    return M_At(array, item_size, index);
}

void *ArrayImpl_AddOne(ARRAY_IMPL *const array, const size_t item_size)
{
    M_Grow(array, item_size, 1);
    return M_At(array, item_size, array->count++);
}

void ArrayImpl_AppendSlice(
    ARRAY_IMPL *const array, const size_t item_size, const void *const items,
    const int32_t count)
{
    if (count == 0) {
        return;
    }
    M_Grow(array, item_size, count);
    memcpy(
        M_At(array, item_size, array->count), items, (size_t)count * item_size);
    array->count += count;
}

void *ArrayImpl_InsertOne(
    ARRAY_IMPL *const array, const size_t item_size, const int32_t index)
{
    ASSERT(index >= 0 && index <= array->count);
    M_Grow(array, item_size, 1);
    char *const target = M_At(array, item_size, index);
    memmove(
        target + item_size, target, (size_t)(array->count - index) * item_size);
    array->count++;
    return target;
}

void *ArrayImpl_Pop(ARRAY_IMPL *const array, const size_t item_size)
{
    ASSERT(array->count > 0);
    array->count--;
    return M_At(array, item_size, array->count);
}

void ArrayImpl_OrderedRemove(
    ARRAY_IMPL *const array, const size_t item_size, const int32_t index)
{
    ASSERT(index >= 0 && index < array->count);
    char *const target = M_At(array, item_size, index);
    memmove(
        target, target + item_size,
        (size_t)(array->count - index - 1) * item_size);
    array->count--;
}

void ArrayImpl_SwapRemove(
    ARRAY_IMPL *const array, const size_t item_size, const int32_t index)
{
    ASSERT(index >= 0 && index < array->count);
    array->count--;
    if (index != array->count) {
        memcpy(
            M_At(array, item_size, index), M_At(array, item_size, array->count),
            item_size);
    }
}
