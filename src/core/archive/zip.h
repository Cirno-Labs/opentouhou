#pragma once

#include "core/archive.h"
#include "core/base/types.h"
#include "core/io/writer.h"

// Reads zip archives as an ARCHIVE_FORMAT, and writes them. Entries that are
// stored or compressed with deflate can be read, and each is checked against
// its CRC-32. ZIP64, encryption and archives of several parts are not
// supported. Names are looked up by the whole path, exactly. The Unix mode
// of an entry is the one the archive records when it was made on Unix, and
// 0 otherwise.

// Sets the largest unpacked size of an entry. An archive with a larger entry
// fails to open.
#define ZIP_MAX_ENTRY_SIZE ((size_t)256 * 1024 * 1024)

extern const ARCHIVE_FORMAT g_ZipFormat;

// Holds one entry for Zip_Write.
typedef struct {
    const char *name;
    const void *data;
    size_t size;
} ZIP_ENTRY;

// Writes entries to out as a zip archive, each one deflated with its
// UTF-8 name and dated 1980-01-01, so that equal entries give equal files.
// Does not close out. Reports failure when an entry or the archive is too
// large for a zip without ZIP64.
RESULT Zip_Write(WRITER *out, const ZIP_ENTRY *entries, size_t count);
