#pragma once

#include "core/archive.h"
#include "core/base/result.h"
#include "core/vfs.h"

// Mounts the data of a game from its folder or from a zip of that folder.

// Returns the one top-level directory, ending with '/', that every entry of
// archive that is not a directory lies below, or "" when there is none, as
// a new string that Mem_Free releases.
char *ShellData_FindRoot(const ARCHIVE *archive);

// Mounts path read-only at the root of files: a directory as it is, and a
// regular file as a zip, from below the root that ShellData_FindRoot gives.
RESULT ShellData_Mount(VFS *files, const char *path);
