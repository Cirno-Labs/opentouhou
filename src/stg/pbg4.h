#pragma once

#include "core/archive.h"

// Reads ZUN's PBG4 archives, the format of th07's data file, as an
// ARCHIVE_FORMAT. The header is the magic, the entry count, the table
// offset and the table's unpacked size. The table is LZSS-packed and holds
// each entry's name, offset, unpacked size and checksum; an entry's data
// ends where the next entry's starts, and the last entry's at the table.
// Entries are found by the last component of a path, as the original does,
// so "data/title00.jpg" finds the entry "title00.jpg"; names are compared
// without regard to ASCII case. Entry names are converted from CP932 to
// UTF-8 when the archive is opened, and entries have no Unix mode. An entry
// is LZSS-packed; its checksum is never checked, as in the original. A bad
// magic, no entries, or a table that runs past the end fails the open.
//
// Example:
// READER file;
// MUST(Vfs_Open(vfs, "th07.dat", VFS_STREAM, &file));
// ARCHIVE *archive;
// MUST(Archive_Open(&g_Pbg4Format, &file, &g_HeapAllocator, &archive));
// Vfs_MountSource(vfs, Archive_ToSource(archive), 100);

extern const ARCHIVE_FORMAT g_Pbg4Format;
