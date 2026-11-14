#pragma once

#include "core/archive.h"
#include "core/base/types.h"
#include "core/io/reader.h"

// Defines what an archive format implements for core/archive. Only the
// formats include this header; every other user sees core/archive.h only.
// A format keeps its record of an entry private: core/archive copies it as
// bytes and gives it back, and never reads it.

// Picks how Archive_Find, and the VFS source, look entry names up.
typedef enum {
    // Compares the whole path with the entry's name, exactly.
    ARCHIVE_LOOKUP_PATH,
    // Compares the last component of the path with the entry's name, with
    // ASCII letters of either case equal, so "data/Title00.jpg" finds the
    // entry "title00.jpg".
    ARCHIVE_LOOKUP_LAST_NOCASE,
} ARCHIVE_LOOKUP;

// Collects the entries that read_table finds.
typedef struct ARCHIVE_TABLE ARCHIVE_TABLE;

// Adds an entry named name, of size unpacked bytes and Unix mode mode (or
// 0). data is data_size bytes of the format's own record for the entry, such
// as where it lies, how it is packed and its check value; read_entry
// receives it back. Both name and data are copied.
void ArchiveTable_Add(
    ARCHIVE_TABLE *table, const char *name, size_t size, uint32_t mode,
    const void *data, size_t data_size);

struct ARCHIVE_FORMAT {
    ARCHIVE_LOOKUP lookup;
    // Reads the entry table of archive and adds every entry to table. A
    // failure needs no archive name: core/archive adds it.
    RESULT (*read_table)(READER *archive, ARCHIVE_TABLE *table);
    // Unpacks the entry whose record is data into out, which has room for
    // exactly size bytes, the size that read_table gave, and checks it. A
    // failure needs no archive or entry name: core/archive adds them.
    RESULT (*read_entry)(
        READER *archive, const void *data, uint8_t *out, size_t size);
};
