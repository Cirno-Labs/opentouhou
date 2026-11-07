#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/vfs/types.h"

// Prepares an empty VFS: nothing is mounted, and no file is found until
// something is.
VFS *Vfs_Create(const ALLOCATOR *allocator);

// Closes every mounted source and releases the VFS.
void Vfs_Destroy(VFS *vfs);

// Mounts source at priority. The VFS takes ownership: Vfs_Destroy, and
// nothing else, closes it. A lookup goes to the highest-priority mounted
// source that has the file; between sources mounted at the same priority,
// the one mounted later wins.
void Vfs_MountSource(VFS *vfs, VFS_SOURCE *source, int32_t priority);

// Mounts the directory at path as a source, at priority, as Vfs_MountSource
// does. Lookups into it are case-insensitive, also on a case-sensitive file
// system: at mount, the directory tree is walked once and every path in it
// is recorded, so a later lookup costs no further access to the disk. A
// directory that does not exist, or that cannot be walked, is a FAIL.
RESULT Vfs_MountDir(VFS *vfs, const char *path, int32_t priority);
