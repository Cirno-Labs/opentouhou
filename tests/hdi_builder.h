#pragma once

// Builds PC-98 hard disk images in Anex86's .hdi format in memory for the
// tests: 17 sectors a track, 4 heads, and one FAT partition from cylinder 1,
// after an inactive entry that points at cylinder 0.

#include "tests/test.h"

#include "core/base/memory.h"

#include <string.h>

#define HDI_BUILDER_HEADER_SIZE 4096
#define HDI_BUILDER_TRACK_SECTORS 17
#define HDI_BUILDER_HEADS 4
#define HDI_BUILDER_ROOT_ENTRIES 64

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

static void M_HdiPut16(uint8_t *const out, const uint32_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

static void M_HdiPut32(uint8_t *const out, const uint32_t value)
{
    M_HdiPut16(out, value & 0xFFFF);
    M_HdiPut16(out + 2, value >> 16);
}

// Sets the FAT entry of cluster in the FAT at fat.
[[maybe_unused]] static void HdiBuilder_SetFat(
    uint8_t *const fat, const bool fat16, const uint32_t cluster,
    const uint32_t value)
{
    if (fat16) {
        M_HdiPut16(fat + 2 * cluster, value);
        return;
    }
    uint8_t *const at = fat + cluster * 3 / 2;
    if ((cluster & 1) != 0) {
        at[0] = (uint8_t)((at[0] & 0x0F) | (value & 0x0F) << 4);
        at[1] = (uint8_t)(value >> 4);
    } else {
        at[0] = (uint8_t)value;
        at[1] = (uint8_t)((at[1] & 0xF0) | (value >> 8 & 0x0F));
    }
}

static void M_HdiPutEntry(
    uint8_t *const out, const char *const name, const uint8_t attributes,
    const uint32_t cluster, const uint32_t size)
{
    memset(out, ' ', 11);
    size_t i = 0;
    for (size_t at = 0; name[i] == '.' && at < 2;) {
        out[at++] = (uint8_t)name[i++];
    }
    for (size_t at = 0;
         name[i] != '\0' && name[i] != '.' && name[i] != '/' && at < 8;) {
        out[at++] = (uint8_t)name[i++];
    }
    if (name[i] == '.') {
        i++;
        for (size_t at = 8; name[i] != '\0' && at < 11;) {
            out[at++] = (uint8_t)name[i++];
        }
    }
    out[11] = attributes;
    M_HdiPut16(out + 26, cluster);
    M_HdiPut32(out + 28, size);
}

// Returns an image of count entries with sector_size-byte sectors, in a
// FAT16 partition when fat16 is true and in a FAT12 one otherwise. Files
// take every other cluster, so that their chains are not contiguous.
[[maybe_unused]] static HDI_BUILDER_IMAGE HdiBuilder_Build(
    const uint32_t sector_size, const bool fat16,
    const HDI_BUILDER_ENTRY *const entries, const int count)
{
    const uint32_t clusters = fat16 ? 4200 : 400;
    const uint32_t fat_bytes =
        fat16 ? (clusters + 2) * 2 : (clusters + 2) * 3 / 2 + 1;
    const uint32_t fat_sectors = (fat_bytes + sector_size - 1) / sector_size;
    const uint32_t root_sectors = HDI_BUILDER_ROOT_ENTRIES * 32 / sector_size;
    const uint32_t data_sector = 1 + 2 * fat_sectors + root_sectors;
    const uint32_t total = data_sector + clusters;
    const uint32_t cylinder_sectors =
        HDI_BUILDER_TRACK_SECTORS * HDI_BUILDER_HEADS;
    const uint32_t cylinders =
        1 + (total + cylinder_sectors - 1) / cylinder_sectors;
    const size_t disk_size = (size_t)cylinders * cylinder_sectors * sector_size;
    HDI_BUILDER_IMAGE image = {
        .size = HDI_BUILDER_HEADER_SIZE + disk_size,
        .fat16 = fat16,
    };
    image.data = Mem_AllocZ(image.size);
    uint8_t *const header = image.data;
    M_HdiPut32(header + 0x08, HDI_BUILDER_HEADER_SIZE);
    M_HdiPut32(header + 0x0C, (uint32_t)disk_size);
    M_HdiPut32(header + 0x10, sector_size);
    M_HdiPut32(header + 0x14, HDI_BUILDER_TRACK_SECTORS);
    M_HdiPut32(header + 0x18, HDI_BUILDER_HEADS);
    M_HdiPut32(header + 0x1C, cylinders);
    uint8_t *const disk = image.data + HDI_BUILDER_HEADER_SIZE;
    uint8_t *const table = disk + sector_size;
    table[0] = 0x21;
    table[1] = 0x01;
    table[32] = 0xA1;
    table[33] = 0x81;
    M_HdiPut16(table + 32 + 6, 1);
    M_HdiPut16(table + 32 + 10, 1);
    M_HdiPut16(table + 32 + 14, cylinders - 1);
    memcpy(table + 32 + 16, "MS-DOS 3.30     ", 16);
    const size_t start = (size_t)cylinder_sectors * sector_size;
    uint8_t *const boot = disk + start;
    M_HdiPut16(boot + 11, sector_size);
    boot[13] = 1;
    M_HdiPut16(boot + 14, 1);
    boot[16] = 2;
    M_HdiPut16(boot + 17, HDI_BUILDER_ROOT_ENTRIES);
    M_HdiPut16(boot + 19, total);
    boot[21] = 0xF8;
    M_HdiPut16(boot + 22, fat_sectors);
    image.fat_offset = HDI_BUILDER_HEADER_SIZE + start + sector_size;
    image.data_offset =
        HDI_BUILDER_HEADER_SIZE + start + (size_t)data_sector * sector_size;
    uint8_t *const fat = image.data + image.fat_offset;
    HdiBuilder_SetFat(fat, fat16, 0, fat16 ? 0xFFF8 : 0xFF8);
    HdiBuilder_SetFat(fat, fat16, 1, fat16 ? 0xFFFF : 0xFFF);
    uint8_t *const root = fat + 2 * (size_t)fat_sectors * sector_size;
    uint8_t *const data = image.data + image.data_offset;
    M_HdiPutEntry(root, "TESTDISK", 0x08, 0, 0);
    uint32_t root_count = 1;
    uint32_t *const cluster_of = Mem_AllocArrZ((size_t)count, sizeof(uint32_t));
    uint32_t *const slots = Mem_AllocArrZ((size_t)count, sizeof(uint32_t));
    uint32_t next = 2;
    for (int i = 0; i < count; i++) {
        const HDI_BUILDER_ENTRY *const entry = &entries[i];
        const size_t length = strlen(entry->name);
        const bool directory = entry->name[length - 1] == '/';
        size_t parent_length = length - (directory ? 1 : 0);
        while (parent_length > 0 && entry->name[parent_length - 1] != '/') {
            parent_length--;
        }
        uint8_t *slot = root + 32 * root_count++;
        for (int p = 0; p < i && parent_length > 0; p++) {
            if (strlen(entries[p].name) == parent_length
                && memcmp(entries[p].name, entry->name, parent_length) == 0) {
                root_count--;
                slot = data + (size_t)(cluster_of[p] - 2) * sector_size
                    + 32 * slots[p]++;
            }
        }
        const char *const name = entry->name + parent_length;
        if (entry->deleted) {
            M_HdiPutEntry(slot, name, 0x20, 0xFFFF, (uint32_t)entry->size);
            slot[0] = 0xE5;
            continue;
        }
        if (directory) {
            cluster_of[i] = next;
            HdiBuilder_SetFat(fat, fat16, next, 0xFFFF);
            M_HdiPutEntry(slot, name, 0x10, next, 0);
            uint8_t *const own = data + (size_t)(next - 2) * sector_size;
            M_HdiPutEntry(own, ".", 0x10, next, 0);
            M_HdiPutEntry(own + 32, "..", 0x10, 0, 0);
            slots[i] = 2;
            next += 2;
            continue;
        }
        const uint32_t first = entry->size > 0 ? next : 0;
        M_HdiPutEntry(slot, name, 0x20, first, (uint32_t)entry->size);
        for (size_t done = 0; done < entry->size; done += sector_size) {
            const size_t part = entry->size - done < sector_size
                ? entry->size - done
                : sector_size;
            memcpy(
                data + (size_t)(next - 2) * sector_size,
                (const uint8_t *)entry->data + done, part);
            const bool last = done + sector_size >= entry->size;
            HdiBuilder_SetFat(fat, fat16, next, last ? 0xFFFF : next + 2);
            next += 2;
        }
    }
    CHECK(next < clusters + 2);
    memcpy(
        fat + (size_t)fat_sectors * sector_size, fat,
        (size_t)fat_sectors * sector_size);
    Mem_Free(slots);
    Mem_Free(cluster_of);
    return image;
}
