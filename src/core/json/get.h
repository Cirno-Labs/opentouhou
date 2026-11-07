#pragma once

#include "core/base/types.h"
#include "core/json/types.h"

// Look up values in a tree. Every lookup accepts nullptr for the value it
// looks in, and then returns nullptr or the fallback, as it does for a value
// of the wrong type. A chain of lookups therefore needs one check, at its
// end.
//
// Example, reading the first server from a patch.js of the form
// { "update": { "servers": ["https://...", ...] } }:
// const JSON_VALUE *const update = Json_Get(patch, "update");
// const JSON_VALUE *const servers = Json_Get(update, "servers");
// const JSON_VALUE *const first = Json_At(servers, 0);
// const char *const server = Json_GetString(first, nullptr);
// if (server == nullptr) {
//     return FAIL("patch.js names no server");
// }

// Returns the value of the member with the given key, or nullptr if object
// is not an object or has no such member. For duplicate keys, the last
// member wins. The search is linear.
const JSON_VALUE *Json_Get(const JSON_VALUE *object, const char *key);

// Returns the item at index, or nullptr if array is not an array or index is
// out of range.
const JSON_VALUE *Json_At(const JSON_VALUE *array, int32_t index);

// Return the value of a boolean, an integer, a number or a string, or the
// fallback for a value of another type. Json_GetInt returns the fallback for
// a number that is not an integer (see JSON_VALUE), and Json_GetNumber
// returns integers too, as doubles. Json_GetString returns the
// NUL-terminated data of the string.
bool Json_GetBool(const JSON_VALUE *value, bool fallback);
int64_t Json_GetInt(const JSON_VALUE *value, int64_t fallback);
double Json_GetNumber(const JSON_VALUE *value, double fallback);
const char *Json_GetString(const JSON_VALUE *value, const char *fallback);
