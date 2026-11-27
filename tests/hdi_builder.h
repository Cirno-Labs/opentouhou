#pragma once

// Builds PC-98 hard disk images in Anex86's .hdi format in memory for the
// tests: 17 sectors a track, 4 heads, and one FAT partition from cylinder 1,
// after an inactive entry that points at cylinder 0.

#include <stddef.h>
#include <stdint.h>

// Holds one entry to put in an image: a file, or a directory when the name
// ends with '/'. Names are 8.3 in upper case, with '/' separators, and the
// parent of an entry comes before it. A deleted entry is written as one.
typedef struct {
    const char *name;
    const void *data;
    size_t size;
    bool deleted;
} HDI_BUILDER_ENTRY;

// Holds an image that HdiBuilder_Build made. Mem_Free releases data.
// fat_offset is the position of the first FAT and data_offset that of
// cluster 2.
typedef struct {
    uint8_t *data;
    size_t size;
    size_t fat_offset;
    size_t data_offset;
    bool fat16;
} HDI_BUILDER_IMAGE;

// Sets the FAT entry of cluster in the FAT at fat.
void HdiBuilder_SetFat(
    uint8_t *fat, bool fat16, uint32_t cluster, uint32_t value);

// Returns an image of count entries with sector_size-byte sectors, in a
// FAT16 partition when fat16 is true and in a FAT12 one otherwise. Files
// take every other cluster, so that their chains are not contiguous.
HDI_BUILDER_IMAGE HdiBuilder_Build(
    uint32_t sector_size, bool fat16, const HDI_BUILDER_ENTRY *entries,
    int count);
