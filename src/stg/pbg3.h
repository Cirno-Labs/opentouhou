#pragma once

#include "core/archive.h"

// Reads ZUN's PBG3 archives, the format of th06's data files, as an
// ARCHIVE_FORMAT. Entries are found by the last component of a path, as the
// original does, so "data/title00.jpg" finds the entry "title00.jpg"; names
// are compared without regard to ASCII case. Entry names are converted from
// CP932 to UTF-8 when the archive is opened, and entries have no Unix mode.
// An entry is LZSS-packed and checked against the sum of its packed bytes.
// A bad magic, or a table that runs past the end, fails the open.
//
// Example:
// READER file;
// MUST(Vfs_Open(vfs, "紅魔郷CM.DAT", VFS_STREAM, &file));
// ARCHIVE *archive;
// MUST(Archive_Open(&g_Pbg3Format, &file, &g_HeapAllocator, &archive));
// Vfs_MountSource(vfs, Archive_ToSource(archive), 100);

extern const ARCHIVE_FORMAT g_Pbg3Format;
