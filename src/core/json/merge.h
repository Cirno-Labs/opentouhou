// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/memory.h"
#include "core/json/types.h"

// Merges source into target as thcrap merges the JSON files of a patch
// stack. Each member of source, in order, does one of three things:
// - When target has a member with the same key and both values are objects,
//   source's object is merged into target's in the same way.
// - When target has a member with the same key otherwise, source's value
//   replaces target's. An array replaces an array; their items are not
//   joined.
// - When target has no member with the key, the member is added after the
//   last member of target.
// When target has duplicate keys, the last member with the key is the one
// merged into, which is the one that Json_Get returns.
//
// Everything that target takes from source is copied with allocator, so
// source can be released afterwards. Blocks that target no longer uses are
// not released, because they can be part of the block from Json_Parse. The
// allocator is meant to be the arena that holds target. When target or
// source is nullptr or not an object, nothing changes.
void Json_Merge(
    JSON_VALUE *target, const JSON_VALUE *source, const ALLOCATOR *allocator);
