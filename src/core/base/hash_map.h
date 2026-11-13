#pragma once

#include "core/base/memory.h"
#include "core/base/types.h"

// Maps keys to values, with open addressing and its memory from an
// ALLOCATOR. HASH_MAP(key type, value type) declares the struct. Every use
// of HASH_MAP is a distinct type, so each pair of types gets one typedef.
//
// Example:
// typedef HASH_MAP(FONT_KEY, FONT) FONT_CACHE;
// FONT_CACHE cache;
// HashMap_Init(&cache, &g_HeapAllocator);
// bool found;
// FONT *const font = HashMap_GetOrPut(&cache, key, &found);
// HashMap_Free(&cache);
//
// A key is one of two kinds, chosen by its type:
// - char * or const char *: a NUL-terminated string, which the map copies
//   and owns. keys[i] is the copy.
// - Any other type: compared and hashed byte by byte. A struct key must
//   have no padding bytes, whose contents are unspecified.
//
// Adding or removing an entry may move the others, which makes pointers to
// keys and values invalid. The hash has no random seed, so the order of
// iteration depends only on the keys and the order of the operations. A map
// whose allocator comes from Arena_Allocator can be part of the simulation
// state.
//
// The HashMap_ macros work on every HASH_MAP type. They take the key and
// value types from the map, so a key or value of another type fails to
// compile, and they evaluate each argument once. They call the one generic
// implementation, the HashMapImpl_ functions, which are not meant to be
// called directly.
#define HASH_MAP(key_, value_)                                                 \
    struct {                                                                   \
        uint64_t *hashes;                                                      \
        key_ *keys;                                                            \
        value_ *values;                                                        \
        int32_t count;                                                         \
        int32_t capacity;                                                      \
        const ALLOCATOR *allocator;                                            \
    }

// Prepares an empty map. It allocates nothing until the first entry is put.
#define HashMap_Init(map_, allocator_)                                         \
    HashMapImpl_Init(HASH_MAP_IMPL_OF(map_), (allocator_))

// Releases every entry and the storage, and leaves the map empty, ready for
// reuse with the same allocator.
#define HashMap_Free(map_)                                                     \
    HashMapImpl_Free(HASH_MAP_IMPL_OF(map_), HASH_MAP_LAYOUT_OF(map_))

// Returns a pointer to the value of a key, or nullptr if the map does not
// contain the key.
#define HashMap_Get(map_, key_)                                                \
    ((typeof((map_)->values))HashMapImpl_Get(                                  \
        HASH_MAP_IMPL_OF(map_), HASH_MAP_LAYOUT_OF(map_),                      \
        HASH_MAP_KEY(map_, key_)))

// Returns whether the map contains a key.
#define HashMap_Contains(map_, key_)                                           \
    (HashMapImpl_Get(                                                          \
         HASH_MAP_IMPL_OF(map_), HASH_MAP_LAYOUT_OF(map_),                     \
         HASH_MAP_KEY(map_, key_))                                             \
     != nullptr)

// Returns a pointer to the value of a key, and adds the key with a value
// filled with zeros if the map does not contain it. An optional bool pointer
// receives whether the key was there already.
#define HashMap_GetOrPut(map_, key_, ...)                                      \
    ((typeof((map_)->values))HashMapImpl_GetOrPut(                             \
        HASH_MAP_IMPL_OF(map_), HASH_MAP_LAYOUT_OF(map_),                      \
        HASH_MAP_KEY(map_, key_),                                              \
        HASH_MAP_FIRST(__VA_ARGS__ __VA_OPT__(, ) nullptr)))

// Stores a copy of value for a key, replacing the value that the key had.
#define HashMap_Put(map_, key_, value_)                                        \
    ((void)(*HashMap_GetOrPut(map_, key_) = (value_)))

// Removes the entry of a key. Returns whether the map contained the key.
#define HashMap_Remove(map_, key_)                                             \
    HashMapImpl_Remove(                                                        \
        HASH_MAP_IMPL_OF(map_), HASH_MAP_LAYOUT_OF(map_),                      \
        HASH_MAP_KEY(map_, key_))

// Removes every entry and keeps the storage.
#define HashMap_Clear(map_)                                                    \
    HashMapImpl_Clear(HASH_MAP_IMPL_OF(map_), HASH_MAP_LAYOUT_OF(map_))

// Moves *index to the next slot from *index onwards that holds an entry.
// Returns false when no slot is left. The map must not change during an
// iteration.
//
// Example:
// for (int32_t i = 0; HashMap_Next(&cache, &i); i++) {
//     use(cache.keys[i], cache.values[i]);
// }
#define HashMap_Next(map_, index_)                                             \
    HashMapImpl_Next(HASH_MAP_IMPL_OF(map_), (index_))

// The rest of this header is the generic implementation behind the macros.

typedef HASH_MAP(void, void) HASH_MAP_IMPL;

typedef struct {
    size_t key_size;
    size_t value_size;
    bool string_keys;
} HASH_MAP_LAYOUT;

#define HASH_MAP_IMPL_OF(map_) ((HASH_MAP_IMPL *)(map_))
#define HASH_MAP_LAYOUT_OF(map_)                                               \
    ((HASH_MAP_LAYOUT) {                                                       \
        .key_size = sizeof(*(map_)->keys),                                     \
        .value_size = sizeof(*(map_)->values),                                 \
        .string_keys = _Generic(                                               \
            *(map_)->keys, char *: true, const char *: true, default: false),  \
    })

// Evaluates to a pointer to a copy of key, converted to the key type of the
// map. A key that does not convert fails to compile.
#define HASH_MAP_KEY(map_, key_) ((typeof (*(map_)->keys)[1]) { (key_) })

#define HASH_MAP_FIRST(first_, ...) first_

void HashMapImpl_Init(HASH_MAP_IMPL *map, const ALLOCATOR *allocator);
void HashMapImpl_Free(HASH_MAP_IMPL *map, HASH_MAP_LAYOUT layout);
void *HashMapImpl_Get(
    const HASH_MAP_IMPL *map, HASH_MAP_LAYOUT layout, const void *key);
void *HashMapImpl_GetOrPut(
    HASH_MAP_IMPL *map, HASH_MAP_LAYOUT layout, const void *key,
    bool *found_existing);
bool HashMapImpl_Remove(
    HASH_MAP_IMPL *map, HASH_MAP_LAYOUT layout, const void *key);
void HashMapImpl_Clear(HASH_MAP_IMPL *map, HASH_MAP_LAYOUT layout);
bool HashMapImpl_Next(const HASH_MAP_IMPL *map, int32_t *index);
