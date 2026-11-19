#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/vfs/types.h"

// Prepares an empty VFS: nothing is mounted, and no file is found until
// something is.
VFS *Vfs_Create(const ALLOCATOR *allocator);

// Closes every mounted source and releases the VFS.
void Vfs_Destroy(VFS *vfs);

// Mounts source over the paths below mount_at, a normalised directory
// path, or "" for every path, at priority. The VFS takes ownership:
// Vfs_Destroy, and nothing else, closes it. A lookup, a list or a write
// considers only the sources whose mount point covers the path, and passes
// each the path below its mount point. A lookup goes to the
// highest-priority source that has the file; between sources mounted at the
// same priority, the one mounted later wins.
void Vfs_MountSource(
    VFS *vfs, const char *mount_at, VFS_SOURCE *source, int32_t priority);

// Mounts the directory at path, as Vfs_MountSource does, under the name
// path. Lookups ignore the case of ASCII letters, also on a case-sensitive
// file system. A read-only directory is read once, here; a writable one at
// each lookup, list and write. A writable directory takes writes, creating
// or replacing files and the directories that lead to them, each part
// spelt as the disk has it in any case, else as the path gives it. Reports
// failure when path is not a directory.
RESULT Vfs_MountDir(
    VFS *vfs, const char *mount_at, const char *path, int32_t priority,
    bool writable);
