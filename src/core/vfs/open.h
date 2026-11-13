#pragma once

#include "core/base/result.h"
#include "core/io/reader.h"
#include "core/vfs/types.h"

// Returns whether a lookup for path would find a file.
bool Vfs_Exists(const VFS *vfs, const char *path);

// Opens path from the highest-priority mounted source that has it, in mode.
// A path that no mounted source has is a FAIL naming the path, both as
// given and normalised, and every source that was searched.
//
// '\\' is read as '/', ASCII letters are read without regard to case, and
// repeated or leading '/' are ignored, so "Data\\Title00.jpg" and
// "data/title00.jpg" name the same file. A path with a ".." component, or an
// absolute path such as "C:/windows", is a FAIL: a game path never escapes
// a mounted directory.
RESULT Vfs_Open(VFS *vfs, const char *path, VFS_MODE mode, READER *out);

// Runs on every file a VFS_IN_MEMORY open returns, before the caller sees
// it, and may replace what the READER holds (core/patch uses this to apply
// thcrap jdiffs). A file opened as VFS_STREAM never goes through the hook,
// because the hook needs the whole file in memory to patch it. A hook that
// fails closes the READER and fails the open.
typedef RESULT (*VFS_HOOK)(void *context, const char *path, READER *file);

// Sets the hook that every VFS_IN_MEMORY open runs through, along with the
// context it receives. A vfs starts with no hook. Setting it again replaces
// both the hook and the context; there is one hook, not a list.
void Vfs_SetHook(VFS *vfs, VFS_HOOK hook, void *context);
