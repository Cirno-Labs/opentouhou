#pragma once

#include "core/base/hash_map.h"
#include "core/base/memory.h"
#include "core/json/types.h"

// Maps the keys of one object to their values, so that a lookup takes
// constant time instead of the linear time of Json_Get. It is meant for
// objects with many members, such as stringdefs.js with thousands of
// strings.
//
// Example:
// JSON_INDEX strings;
// Json_BuildIndex(stringdefs, alloc, &strings);
// const char *text = Json_GetString(Json_IndexGet(&strings, "th06_title"), "");
// ...
// HashMap_Free(&strings);
typedef HASH_MAP(const char *, const JSON_VALUE *) JSON_INDEX;

// Prepares an index of the members of object, with its memory and its copies
// of the keys from allocator. The values point into the tree, which must
// outlive the index. For duplicate keys, the last member wins, as for
// Json_Get. When object is nullptr or not an object, the index is empty. A
// Json_Merge into object afterwards keeps the index valid, but the members
// that it adds are not in the index. HashMap_Free releases the index.
void Json_BuildIndex(
    const JSON_VALUE *object, const ALLOCATOR *allocator, JSON_INDEX *out);

// Returns the value of key in the index, or nullptr when the index does not
// contain key. A nullptr index or key returns nullptr, so that the result can
// go straight to a getter.
const JSON_VALUE *Json_IndexGet(const JSON_INDEX *index, const char *key);
