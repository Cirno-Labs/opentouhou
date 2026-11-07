#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/vfs.h"

// Builds a VFS_SOURCE over a directory on disk: at creation, the directory
// tree is walked once and every path in it is recorded, normalised, so a
// later lookup costs no further access to the disk. path becomes the
// source's name. A directory that does not exist, or that cannot be walked,
// is a FAIL, and *out is left at nullptr.
RESULT DirSource_Create(
    const char *path, const ALLOCATOR *allocator, VFS_SOURCE **out);
