#pragma once

#include "core/base/arena.h"
#include "core/base/array.h"
#include "core/base/result.h"
#include "core/base/types.h"

// Reads the entries of a zip archive that is held in memory. Entries that
// are stored or compressed with deflate can be read. ZIP64, encryption and
// archives of several parts are not supported.

// Sets the largest unpacked size of an entry that Zip_Read accepts.
#define ZIP_MAX_ENTRY_SIZE ((size_t)256 * 1024 * 1024)

// Holds one entry of the central directory of an archive.
typedef struct {
    // Holds the name, a path with '/' separators. The name of a directory
    // ends with '/'.
    const char *name;
    // Holds the Unix mode, such as 0100755 for an executable file, or 0 when
    // the archive was not made on Unix.
    uint32_t mode;
    // Hold the fields that Zip_Read uses.
    uint16_t method;
    uint32_t crc;
    uint32_t packed_size;
    uint32_t size;
    uint32_t header_offset;
} ZIP_ENTRY;

typedef ARRAY(ZIP_ENTRY) ZIP_ENTRY_LIST;

// Lists the entries of the archive in size bytes of data, in the order of
// its central directory. The list and the names come from arena.
RESULT Zip_List(
    const uint8_t *data, size_t size, ARENA *arena, ZIP_ENTRY_LIST *out);

// Returns the entry of list with the given name, or nullptr when there is
// none.
const ZIP_ENTRY *Zip_Find(const ZIP_ENTRY_LIST *list, const char *name);

// Unpacks an entry that Zip_List found in the same data, and checks its
// CRC-32. Mem_Free releases *out. On failure, *out is nullptr and *out_size
// is 0.
RESULT Zip_Read(
    const uint8_t *data, size_t size, const ZIP_ENTRY *entry, uint8_t **out,
    size_t *out_size);
