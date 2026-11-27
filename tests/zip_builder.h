#pragma once

// Builds zip archives in memory for the tests.

#include <stddef.h>
#include <stdint.h>

// Holds one entry to put in an archive. A mode of 0 makes the archive
// entry come from a system that is not Unix.
typedef struct {
    const char *name;
    const void *data;
    size_t size;
    uint32_t mode;
    bool deflate;
} ZIP_BUILDER_ENTRY;

// Holds an archive that ZipBuilder_Build made. Mem_Free releases data.
typedef struct {
    uint8_t *data;
    size_t size;
} ZIP_BUILDER_ARCHIVE;

// Returns an archive of count entries.
ZIP_BUILDER_ARCHIVE ZipBuilder_Build(
    const ZIP_BUILDER_ENTRY *entries, int count);

// Writes value at *out in little-endian order and moves *out past it.
void ZipBuilder_Put32(uint8_t **out, uint32_t value);
