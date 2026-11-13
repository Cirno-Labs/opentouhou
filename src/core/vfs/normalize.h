// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"

// Normalises a VFS path into *out, a new string from allocator that
// Allocator_Free(allocator, *out, strlen(*out) + 1) releases. '\\' becomes
// '/', ASCII letters are lowercased, and repeated or leading '/' are
// dropped. A path with a ".." component, or an absolute path such as
// "C:/windows", is a FAIL, and *out is left at nullptr.
RESULT VfsNormalize_Path(
    const char *path, const ALLOCATOR *allocator, char **out);
