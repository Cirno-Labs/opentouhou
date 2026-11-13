// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/io/reader.h"
#include "core/vfs.h"

// Reads ZUN's PBG3 archives, the format of th06's data files, as a VFS
// source. Entries are found by the last component of a path, as the
// original does, so "data/title00.jpg" finds the entry "title00.jpg";
// names are compared without regard to ASCII case. Entry names are
// converted from CP932 to UTF-8 when the archive is opened. An entry is
// decompressed whole when it is opened, so every open returns it in memory.
//
// Example:
// MUST(Vfs_MountDir(vfs, data_dir, 0));
// MUST(Vfs_MountFile(vfs, "紅魔郷CM.DAT", Pbg3_Open, 100));

// Reads the header and the entry table of the PBG3 archive in archive and
// returns a VFS source named name over its entries, matching VFS_OPENER.
// The source takes ownership of the reader and closes it when the VFS
// closes the source. A bad magic, or a table that runs past the end, is a
// failure naming the archive, and the reader is closed.
RESULT Pbg3_Open(
    READER *archive, const char *name, const ALLOCATOR *allocator,
    VFS_SOURCE **out);
