#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/io/reader.h"
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

// Builds a VFS source from an opened file, such as Pbg3_Open. name is the
// path the file was opened as; the source keeps a copy as its name, and
// every failure names the file by it. The allocator is the VFS's own. On
// failure, the opener has closed file.
typedef RESULT (*VFS_OPENER)(
    READER *file, const char *name, const ALLOCATOR *allocator,
    VFS_SOURCE **out);

// Opens path through vfs as a stream, builds a source named path from it
// with opener, and mounts that source at priority, as Vfs_MountSource does.
// A path that cannot be opened, or that opener refuses, is a FAIL, and
// nothing is mounted.
RESULT Vfs_MountFile(
    VFS *vfs, const char *path, VFS_OPENER opener, int32_t priority);
