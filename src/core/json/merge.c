// SPDX-License-Identifier: GPL-3.0-only

#include "core/json.h"

#include "core/base/assert.h"
#include "core/base/hash_map.h"

#include <string.h>

// Sets the member count from which a merge finds the members of target
// through a hash map instead of a linear search.
#define M_INDEX_MIN_COUNT 16

typedef HASH_MAP(const char *, int32_t) MEMBER_INDEX;

static void *M_CopyBytes(
    const ALLOCATOR *const allocator, const void *const data, const size_t size)
{
    void *const copy = Allocator_Alloc(allocator, size);
    memcpy(copy, data, size);
    return copy;
}

static const char *M_CopyKey(
    const ALLOCATOR *const allocator, const char *const key)
{
    return M_CopyBytes(allocator, key, strlen(key) + 1);
}

static JSON_VALUE M_Copy(
    const JSON_VALUE *const source, const ALLOCATOR *const allocator)
{
    JSON_VALUE copy = *source;
    if (source->type == JSON_STRING) {
        copy.string.data = M_CopyBytes(
            allocator, source->string.data, source->string.size + 1);
    } else if (source->type == JSON_ARRAY) {
        const int32_t count = source->array.count;
        copy.array.items = Allocator_Alloc(
            allocator, Mem_ArrSize((size_t)count, sizeof(JSON_VALUE)));
        for (int32_t i = 0; i < count; i++) {
            copy.array.items[i] = M_Copy(&source->array.items[i], allocator);
        }
    } else if (source->type == JSON_OBJECT) {
        const int32_t count = source->object.count;
        JSON_VALUE *const values = Allocator_Alloc(
            allocator, Mem_ArrSize((size_t)count, sizeof(JSON_VALUE)));
        copy.object.members = Allocator_Alloc(
            allocator, Mem_ArrSize((size_t)count, sizeof(JSON_MEMBER)));
        for (int32_t i = 0; i < count; i++) {
            const JSON_MEMBER *const member = &source->object.members[i];
            values[i] = M_Copy(member->value, allocator);
            copy.object.members[i] = (JSON_MEMBER) {
                .key = M_CopyKey(allocator, member->key),
                .value = &values[i],
            };
        }
    }
    return copy;
}

// Returns the index of the last member of members with the key, or -1.
static int32_t M_Find(
    const JSON_MEMBER *const members, const int32_t count,
    const MEMBER_INDEX *const index, const char *const key)
{
    if (index != nullptr) {
        const int32_t *const found = HashMap_Get(index, key);
        return found != nullptr ? *found : -1;
    }
    for (int32_t i = count - 1; i >= 0; i--) {
        if (strcmp(members[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

static void M_Merge(
    JSON_VALUE *const target, const JSON_VALUE *const source,
    const ALLOCATOR *const allocator)
{
    JSON_MEMBER *members = target->object.members;
    int32_t count = target->object.count;
    bool grown = false;

    MEMBER_INDEX index;
    const bool indexed = count >= M_INDEX_MIN_COUNT;
    if (indexed) {
        HashMap_Init(&index, &g_HeapAllocator);
        for (int32_t i = 0; i < count; i++) {
            HashMap_Put(&index, members[i].key, i);
        }
    }

    for (int32_t i = 0; i < source->object.count; i++) {
        const JSON_MEMBER *const member = &source->object.members[i];
        const int32_t found =
            M_Find(members, count, indexed ? &index : nullptr, member->key);
        if (found >= 0) {
            JSON_VALUE *const value = members[found].value;
            if (value->type == JSON_OBJECT
                && member->value->type == JSON_OBJECT) {
                M_Merge(value, member->value, allocator);
            } else {
                *value = M_Copy(member->value, allocator);
            }
            continue;
        }

        // The block of members grows once, with room for every member of
        // source that is left, so that new keys never move it again.
        if (!grown) {
            const size_t capacity =
                (size_t)count + (size_t)(source->object.count - i);
            JSON_MEMBER *const grown_members = Allocator_Alloc(
                allocator, Mem_ArrSize(capacity, sizeof(JSON_MEMBER)));
            if (count > 0) {
                memcpy(
                    grown_members, members, (size_t)count * sizeof(*members));
            }
            members = grown_members;
            grown = true;
        }
        JSON_VALUE *const value = Allocator_Alloc(allocator, sizeof(*value));
        *value = M_Copy(member->value, allocator);
        members[count] = (JSON_MEMBER) {
            .key = M_CopyKey(allocator, member->key),
            .value = value,
        };
        if (indexed) {
            HashMap_Put(&index, members[count].key, count);
        }
        ASSERT(count < INT32_MAX);
        count++;
    }

    if (indexed) {
        HashMap_Free(&index);
    }
    target->object.members = members;
    target->object.count = count;
}

void Json_Merge(
    JSON_VALUE *const target, const JSON_VALUE *const source,
    const ALLOCATOR *const allocator)
{
    if (target == nullptr || source == nullptr || target->type != JSON_OBJECT
        || source->type != JSON_OBJECT) {
        return;
    }
    M_Merge(target, source, allocator);
}
