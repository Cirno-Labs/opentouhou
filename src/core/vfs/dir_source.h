#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/vfs/types.h"

// Builds the source that Vfs_MountDir mounts over the directory at path,
// which becomes its name, into *out. Reports failure when path is not a
// directory. Only core/vfs uses it.

RESULT VfsDirSource_Create(
    const char *path, bool writable, const ALLOCATOR *allocator,
    VFS_SOURCE **out);
