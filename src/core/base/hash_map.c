// SPDX-License-Identifier: GPL-3.0-only

#include "core/base/hash_map.h"

#include "core/base/assert.h"

#include <string.h>

#define M_MIN_CAPACITY 8
#define M_EMPTY 0

static uint64_t M_HashBytes(const void *const data, const size_t size)
{
    const unsigned char *const bytes = data;
    uint64_t hash = 14695981039346656037ULL;
    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }
    // FNV-1a leaves its low bits depending only on the low bits of each
    // byte, and the table index is the low bits. The MurmurHash3 finalizer
    // mixes every bit into them.
    hash ^= hash >> 33;
    hash *= 0xFF51AFD7ED558CCDULL;
    hash ^= hash >> 33;
    hash *= 0xC4CEB9FE1A85EC53ULL;
    hash ^= hash >> 33;
    return hash == M_EMPTY ? 1 : hash;
}

static const char *M_KeyString(const void *const key)
{
    const char *string;
    memcpy(&string, key, sizeof(string));
    return string;
}

static uint64_t M_Hash(const HASH_MAP_LAYOUT layout, const void *const key)
{
    if (layout.string_keys) {
        const char *const string = M_KeyString(key);
        return M_HashBytes(string, strlen(string));
    }
    return M_HashBytes(key, layout.key_size);
}

static void *M_Key(
    const HASH_MAP_IMPL *const map, const HASH_MAP_LAYOUT layout,
    const int32_t index)
{
    return (char *)map->keys + (size_t)index * layout.key_size;
}

static void *M_Value(
    const HASH_MAP_IMPL *const map, const HASH_MAP_LAYOUT layout,
    const int32_t index)
{
    return (char *)map->values + (size_t)index * layout.value_size;
}

static bool M_KeyEquals(
    const HASH_MAP_IMPL *const map, const HASH_MAP_LAYOUT layout,
    const int32_t index, const void *const key)
{
    if (layout.string_keys) {
        return strcmp(M_KeyString(M_Key(map, layout, index)), M_KeyString(key))
            == 0;
    }
    return memcmp(M_Key(map, layout, index), key, layout.key_size) == 0;
}

static int32_t M_Find(
    const HASH_MAP_IMPL *const map, const HASH_MAP_LAYOUT layout,
    const void *const key, const uint64_t hash)
{
    if (map->count == 0) {
        return -1;
    }
    const uint64_t mask = (uint64_t)map->capacity - 1;
    for (uint64_t i = hash & mask;; i = (i + 1) & mask) {
        if (map->hashes[i] == M_EMPTY) {
            return -1;
        }
        if (map->hashes[i] == hash
            && M_KeyEquals(map, layout, (int32_t)i, key)) {
            return (int32_t)i;
        }
    }
}

static int32_t M_FindEmpty(const HASH_MAP_IMPL *const map, const uint64_t hash)
{
    const uint64_t mask = (uint64_t)map->capacity - 1;
    uint64_t i = hash & mask;
    while (map->hashes[i] != M_EMPTY) {
        i = (i + 1) & mask;
    }
    return (int32_t)i;
}

static void M_CopySlot(
    HASH_MAP_IMPL *const target, const int32_t target_index,
    const HASH_MAP_IMPL *const source, const int32_t source_index,
    const HASH_MAP_LAYOUT layout)
{
    target->hashes[target_index] = source->hashes[source_index];
    memcpy(
        M_Key(target, layout, target_index),
        M_Key(source, layout, source_index), layout.key_size);
    memcpy(
        M_Value(target, layout, target_index),
        M_Value(source, layout, source_index), layout.value_size);
}

static void M_FreeStorage(
    HASH_MAP_IMPL *const map, const HASH_MAP_LAYOUT layout)
{
    const size_t capacity = (size_t)map->capacity;
    Allocator_Free(map->allocator, map->hashes, capacity * sizeof(uint64_t));
    Allocator_Free(map->allocator, map->keys, capacity * layout.key_size);
    Allocator_Free(map->allocator, map->values, capacity * layout.value_size);
}

static void M_FreeKey(
    HASH_MAP_IMPL *const map, const HASH_MAP_LAYOUT layout, const int32_t index)
{
    if (layout.string_keys) {
        const char *const string = M_KeyString(M_Key(map, layout, index));
        Allocator_Free(map->allocator, (char *)string, strlen(string) + 1);
    }
}

static void M_Grow(HASH_MAP_IMPL *const map, const HASH_MAP_LAYOUT layout)
{
    // The map grows at three quarters full, so that a probe for a missing
    // key always reaches an empty slot quickly.
    if ((int64_t)(map->count + 1) * 4 <= (int64_t)map->capacity * 3) {
        return;
    }
    ASSERT(map->capacity <= INT32_MAX / 2);

    HASH_MAP_IMPL grown = *map;
    grown.capacity = map->capacity == 0 ? M_MIN_CAPACITY : map->capacity * 2;
    const size_t capacity = (size_t)grown.capacity;
    grown.hashes = Allocator_AllocZ(
        map->allocator, Mem_ArrSize(capacity, sizeof(uint64_t)));
    grown.keys =
        Allocator_Alloc(map->allocator, Mem_ArrSize(capacity, layout.key_size));
    grown.values = Allocator_Alloc(
        map->allocator, Mem_ArrSize(capacity, layout.value_size));
    for (int32_t i = 0; i < map->capacity; i++) {
        if (map->hashes[i] != M_EMPTY) {
            M_CopySlot(
                &grown, M_FindEmpty(&grown, map->hashes[i]), map, i, layout);
        }
    }

    M_FreeStorage(map, layout);
    *map = grown;
}

void HashMapImpl_Init(
    HASH_MAP_IMPL *const map, const ALLOCATOR *const allocator)
{
    *map = (HASH_MAP_IMPL) { .allocator = allocator };
}

void HashMapImpl_Free(HASH_MAP_IMPL *const map, const HASH_MAP_LAYOUT layout)
{
    HashMapImpl_Clear(map, layout);
    M_FreeStorage(map, layout);
    *map = (HASH_MAP_IMPL) { .allocator = map->allocator };
}

void *HashMapImpl_Get(
    const HASH_MAP_IMPL *const map, const HASH_MAP_LAYOUT layout,
    const void *const key)
{
    const int32_t index = M_Find(map, layout, key, M_Hash(layout, key));
    return index < 0 ? nullptr : M_Value(map, layout, index);
}

void *HashMapImpl_GetOrPut(
    HASH_MAP_IMPL *const map, const HASH_MAP_LAYOUT layout,
    const void *const key, bool *const found_existing)
{
    const uint64_t hash = M_Hash(layout, key);
    int32_t index = M_Find(map, layout, key, hash);
    if (found_existing != nullptr) {
        *found_existing = index >= 0;
    }
    if (index >= 0) {
        return M_Value(map, layout, index);
    }

    M_Grow(map, layout);
    index = M_FindEmpty(map, hash);
    map->hashes[index] = hash;
    if (layout.string_keys) {
        const char *const string = M_KeyString(key);
        const size_t size = strlen(string) + 1;
        char *const copy = Allocator_Alloc(map->allocator, size);
        memcpy(copy, string, size);
        memcpy(M_Key(map, layout, index), &copy, sizeof(copy));
    } else {
        memcpy(M_Key(map, layout, index), key, layout.key_size);
    }
    map->count++;

    void *const value = M_Value(map, layout, index);
    memset(value, 0, layout.value_size);
    return value;
}

bool HashMapImpl_Remove(
    HASH_MAP_IMPL *const map, const HASH_MAP_LAYOUT layout,
    const void *const key)
{
    int32_t hole = M_Find(map, layout, key, M_Hash(layout, key));
    if (hole < 0) {
        return false;
    }
    M_FreeKey(map, layout, hole);

    // Moves back each later entry of the probe run whose home slot is not
    // between the hole and the entry, so that no probe meets the hole before
    // it reaches its key.
    const uint64_t mask = (uint64_t)map->capacity - 1;
    for (uint64_t i = ((uint64_t)hole + 1) & mask; map->hashes[i] != M_EMPTY;
         i = (i + 1) & mask) {
        const uint64_t home = map->hashes[i] & mask;
        const uint64_t hole_distance = (i - (uint64_t)hole) & mask;
        const uint64_t home_distance = (i - home) & mask;
        if (home_distance >= hole_distance) {
            M_CopySlot(map, hole, map, (int32_t)i, layout);
            hole = (int32_t)i;
        }
    }
    map->hashes[hole] = M_EMPTY;
    map->count--;
    return true;
}

void HashMapImpl_Clear(HASH_MAP_IMPL *const map, const HASH_MAP_LAYOUT layout)
{
    for (int32_t i = 0; i < map->capacity; i++) {
        if (map->hashes[i] != M_EMPTY) {
            M_FreeKey(map, layout, i);
            map->hashes[i] = M_EMPTY;
        }
    }
    map->count = 0;
}

bool HashMapImpl_Next(const HASH_MAP_IMPL *const map, int32_t *const index)
{
    for (int32_t i = *index; i < map->capacity; i++) {
        if (map->hashes[i] != M_EMPTY) {
            *index = i;
            return true;
        }
    }
    *index = map->capacity;
    return false;
}
