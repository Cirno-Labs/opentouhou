#pragma once

#include "core/archive.h"
#include "core/base/result.h"
#include "core/vfs.h"

// Mounts the data of a game from its folder, a zip of that folder, or a
// PC-98 hard disk image.

// Returns the one top-level directory, ending with '/', that every entry of
// archive that is not a directory lies below, or "" when there is none, as
// a new string that Mem_Free releases.
char *ShellData_FindRoot(const ARCHIVE *archive);

// Mounts path read-only at the root of files: a directory as it is, and a
// regular file as a zip or an .hdi, or a zip that holds one .hdi, from below
// the directory that holds marker (such as "reiiden.cfg"), or
// ShellData_FindRoot's root when marker is nullptr or no entry matches it.
// A file that starts with a zip signature is a zip; any other is an .hdi.
// marker matches the last component of an entry's name with ASCII letters
// of either case equal, and the shallowest match wins.
RESULT ShellData_Mount(VFS *files, const char *path, const char *marker);
