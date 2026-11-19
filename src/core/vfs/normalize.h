#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"

#include <string.h>

// Normalises a VFS path into *out, a new string from allocator that
// Allocator_Free(allocator, *out, strlen(*out) + 1) releases. '\\' becomes
// '/', ASCII letters are lowercased, and repeated or leading '/' are
// dropped. A path with a ".." component, or an absolute path such as
// "C:/windows", is a FAIL, and *out is left at nullptr.
RESULT VfsNormalize_Path(
    const char *path, const ALLOCATOR *allocator, char **out);

// Normalises path as VfsNormalize_Path does, but keeps the case of letters.
RESULT VfsNormalize_PathKeepCase(
    const char *path, const ALLOCATOR *allocator, char **out);

// Returns the part of path below dir, both in normal form, "" being the
// root, or nullptr when path does not lie below dir. Returns "" when path
// is dir.
static inline const char *VfsNormalize_Below(
    const char *const path, const char *const dir)
{
    const size_t length = strlen(dir);
    if (length == 0) {
        return path;
    }
    if (strncmp(path, dir, length) != 0) {
        return nullptr;
    }
    if (path[length] == '\0') {
        return path + length;
    }
    return path[length] == '/' ? path + length + 1 : nullptr;
}

// Returns the name of path when it lies directly below dir, as
// VfsNormalize_Below takes them, or nullptr.
static inline const char *VfsNormalize_NameBelow(
    const char *const path, const char *const dir)
{
    const char *const name = VfsNormalize_Below(path, dir);
    return name != nullptr && strchr(name, '/') == nullptr ? name : nullptr;
}
