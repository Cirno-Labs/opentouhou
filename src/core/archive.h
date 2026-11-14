#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/base/types.h"
#include "core/io/reader.h"
#include "core/vfs/types.h"

// Reads archives through one interface: core/archive/zip.h, stg/pbg3.h and
// each other format provide an ARCHIVE_FORMAT, and every archive behaves the
// same whatever its format. Opening an archive reads its entry table only;
// an entry is unpacked whole when it is read.
//
// Each format looks names up in one of two ways (core/archive/format.h): by
// the whole path exactly, or by the last component of the path with ASCII
// letters of either case equal. When two entries of an archive have the same
// name under that lookup, the later one in the table is found, and both stay
// in the entry list.
//
// Example:
// READER file;
// MUST(Reader_OpenFile(&file, "package.zip"));
// ARCHIVE *archive;
// MUST(Archive_Open(&g_ZipFormat, &file, &g_HeapAllocator, &archive));
// const ARCHIVE_ENTRY *const entry = Archive_Find(archive, "README.txt");
// uint8_t *data;
// MUST(Archive_Read(archive, entry, &data));
// // use data
// Allocator_Free(&g_HeapAllocator, data, ArchiveEntry_GetSize(entry));
// Archive_Close(archive);

typedef struct ARCHIVE_FORMAT ARCHIVE_FORMAT;
typedef struct ARCHIVE ARCHIVE;
typedef struct ARCHIVE_ENTRY ARCHIVE_ENTRY;

// Reads the entry table of the archive in reader, in format. The archive
// takes ownership of the reader; on failure, the reader is closed. The
// archive is named by its reader (Reader_GetName), and has no name when the
// reader has none. A failure starts with the archive's name.
RESULT Archive_Open(
    const ARCHIVE_FORMAT *format, READER *reader, const ALLOCATOR *allocator,
    ARCHIVE **out);

// Closes the reader and releases the archive and its entries.
void Archive_Close(ARCHIVE *archive);

// Return the number of entries, and the entry at index, in the order of the
// archive's table. index must be below the count.
int32_t Archive_GetCount(const ARCHIVE *archive);
const ARCHIVE_ENTRY *Archive_GetEntry(const ARCHIVE *archive, int32_t index);

// Returns the entry that path names, as the format looks names up, or
// nullptr when there is none.
const ARCHIVE_ENTRY *Archive_Find(const ARCHIVE *archive, const char *path);

// Returns the entry's name: UTF-8, with '/' separators. The name of a
// directory ends with '/'.
const char *ArchiveEntry_GetName(const ARCHIVE_ENTRY *entry);

// Returns the entry's unpacked size in bytes.
size_t ArchiveEntry_GetSize(const ARCHIVE_ENTRY *entry);

// Returns the entry's Unix mode, such as 0100755 for an executable file, or
// 0 when the archive records none.
uint32_t ArchiveEntry_GetMode(const ARCHIVE_ENTRY *entry);

// Unpacks entry into a new block of ArchiveEntry_GetSize(entry) bytes from
// the archive's allocator, and checks it as the format does, such as against
// a CRC-32. Allocator_Free(allocator, *out, size) releases the block. A
// failure starts with the archive's name and the entry's name, and *out is
// nullptr.
RESULT Archive_Read(
    ARCHIVE *archive, const ARCHIVE_ENTRY *entry, uint8_t **out);

// Returns a VFS source over the archive, named as the archive, or "archive"
// when the archive has no name. The source takes ownership of the archive and
// closes it when the VFS closes the source. It has every entry that is not a
// directory, at its name with ASCII letters folded to lower case, as the VFS
// normalises paths; a format that looks names up by their last component
// finds them by it also through the source. An entry opens in memory, in a
// reader named "<archive>/<entry>", such as "紅魔郷CM.DAT/title00.jpg", or
// "<entry>" when the archive has no name.
VFS_SOURCE *Archive_ToSource(ARCHIVE *archive);
