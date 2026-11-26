#include "core/archive/hdi.h"

#include "core/archive/format.h"
#include "core/base/cp932.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/io/reader.h"

#include <string.h>

#define M_HEADER_SIZE 32
#define M_PARTITION_SIZE 32
#define M_MAX_PARTITIONS 16
#define M_PARTITION_ACTIVE 0x80
#define M_PARTITION_TYPE 0x7F
#define M_TYPE_FAT12 0x01
#define M_TYPE_FAT16 0x11
#define M_TYPE_FAT16_LARGE 0x21
#define M_ENTRY_SIZE 32
#define M_ENTRY_END 0x00
#define M_ENTRY_DELETED 0xE5
#define M_ENTRY_KANJI_E5 0x05
#define M_ATTRIBUTE_VOLUME 0x08
#define M_ATTRIBUTE_DIRECTORY 0x10
#define M_FAT12_CLUSTERS 4085
#define M_FAT16_CLUSTERS 65525
#define M_FAT12_END 0xFF8
#define M_FAT16_END 0xFFF8
#define M_FIRST_CLUSTER 2
#define M_MAX_DEPTH 32

typedef struct {
    size_t start;
    size_t size;
    uint32_t sector_size;
    uint32_t sectors;
    uint32_t heads;
} M_DISK;

typedef struct {
    size_t root_offset;
    size_t root_size;
    size_t data_offset;
    uint32_t cluster_size;
    uint32_t clusters;
    uint16_t end;
    // Holds the FAT entry of each cluster, from cluster 0.
    uint16_t *next;
    // Marks the clusters read as directories, so that a loop fails.
    bool *seen;
} M_VOLUME;

typedef struct {
    size_t data_offset;
    uint32_t cluster_size;
    uint32_t count;
    uint16_t clusters[];
} HDI_RECORD;

static RESULT M_ReadDisk(READER *const archive, M_DISK *const disk)
{
    Reader_Seek(archive, 0);
    const uint32_t reserved = Reader_ReadU32(archive);
    Reader_Skip(archive, 4);
    const uint32_t header_size = Reader_ReadU32(archive);
    const uint32_t data_size = Reader_ReadU32(archive);
    disk->sector_size = Reader_ReadU32(archive);
    disk->sectors = Reader_ReadU32(archive);
    disk->heads = Reader_ReadU32(archive);
    MUST(Reader_Result(archive));
    const size_t size = Reader_Size(archive);
    FAIL_IF(
        reserved != 0 || header_size < M_HEADER_SIZE || header_size > size,
        "the image has no Anex86 header");
    FAIL_IF(
        disk->sector_size != 256 && disk->sector_size != 512
            && disk->sector_size != 1024 && disk->sector_size != 2048,
        "the sector size %u is not supported", disk->sector_size);
    FAIL_IF(
        disk->sectors == 0 || disk->heads == 0, "the image has no geometry");
    FAIL_IF(
        data_size > size - header_size,
        "the image is shorter than its header says");
    disk->start = header_size;
    disk->size = data_size;
    return OK;
}

// Finds the first active FAT partition in the table in sector 1, and
// returns the position of its first sector.
static RESULT M_FindPartition(
    READER *const archive, const M_DISK *const disk, size_t *const out)
{
    FAIL_IF(
        disk->size < 2 * (size_t)disk->sector_size,
        "the image has no partition table");
    Reader_Seek(archive, disk->start + disk->sector_size);
    const uint32_t count =
        disk->sector_size / M_PARTITION_SIZE < M_MAX_PARTITIONS
        ? disk->sector_size / M_PARTITION_SIZE
        : M_MAX_PARTITIONS;
    for (uint32_t i = 0; i < count; i++) {
        Reader_Skip(archive, 1);
        const uint8_t system = Reader_ReadU8(archive);
        Reader_Skip(archive, 6);
        const uint8_t sector = Reader_ReadU8(archive);
        const uint8_t head = Reader_ReadU8(archive);
        const uint16_t cylinder = Reader_ReadU16(archive);
        Reader_Skip(archive, 20);
        MUST(Reader_Result(archive));
        const uint8_t type = system & M_PARTITION_TYPE;
        if ((system & M_PARTITION_ACTIVE) == 0
            || (type != M_TYPE_FAT12 && type != M_TYPE_FAT16
                && type != M_TYPE_FAT16_LARGE)) {
            continue;
        }
        // PC-98 partition tables count sectors from 0.
        const uint64_t lba =
            ((uint64_t)cylinder * disk->heads + head) * disk->sectors + sector;
        FAIL_IF(
            sector >= disk->sectors || head >= disk->heads
                || lba >= disk->size / disk->sector_size,
            "partition %u starts outside the image", i + 1);
        *out = disk->start + (size_t)lba * disk->sector_size;
        return OK;
    }
    return FAIL("the image has no active FAT12 or FAT16 partition");
}

static RESULT M_ReadFat(
    READER *const archive, const size_t offset, const size_t fat_bytes,
    M_VOLUME *const volume)
{
    Reader_Seek(archive, offset);
    uint8_t *const fat = Reader_ReadAlloc(archive, fat_bytes, &g_HeapAllocator);
    MUST(Reader_Result(archive));
    const uint32_t count = volume->clusters + M_FIRST_CLUSTER;
    volume->next = Mem_AllocArrZ(count, sizeof(*volume->next));
    volume->seen = Mem_AllocArrZ(count, sizeof(*volume->seen));
    for (uint32_t i = 0; i < count; i++) {
        if (volume->end == M_FAT16_END) {
            volume->next[i] = (uint16_t)(fat[2 * i] | fat[2 * i + 1] << 8);
        } else {
            const size_t at = (size_t)i * 3 / 2;
            const uint16_t pair = (uint16_t)(fat[at] | fat[at + 1] << 8);
            volume->next[i] = (i & 1) != 0 ? pair >> 4 : pair & 0xFFF;
        }
    }
    Allocator_Free(&g_HeapAllocator, fat, fat_bytes);
    return OK;
}

static RESULT M_ReadVolume(
    READER *const archive, const M_DISK *const disk, const size_t start,
    M_VOLUME *const volume)
{
    Reader_Seek(archive, start + 11);
    const uint16_t sector_size = Reader_ReadU16(archive);
    const uint8_t cluster_sectors = Reader_ReadU8(archive);
    const uint16_t reserved = Reader_ReadU16(archive);
    const uint8_t fats = Reader_ReadU8(archive);
    const uint16_t root_entries = Reader_ReadU16(archive);
    const uint16_t total_small = Reader_ReadU16(archive);
    Reader_Skip(archive, 1);
    const uint16_t fat_sectors = Reader_ReadU16(archive);
    Reader_Skip(archive, 8);
    const uint32_t total_large = Reader_ReadU32(archive);
    MUST(Reader_Result(archive));
    FAIL_IF(
        sector_size < 128 || sector_size > 4096
            || (sector_size & (sector_size - 1)) != 0 || cluster_sectors == 0
            || (cluster_sectors & (cluster_sectors - 1)) != 0 || reserved == 0
            || fats == 0 || fat_sectors == 0 || root_entries == 0,
        "the partition has no FAT boot sector");
    const uint64_t total = total_small != 0 ? total_small : total_large;
    const uint64_t root_sectors =
        ((uint64_t)root_entries * M_ENTRY_SIZE + sector_size - 1) / sector_size;
    const uint64_t data_sector =
        reserved + (uint64_t)fats * fat_sectors + root_sectors;
    FAIL_IF(data_sector >= total, "the partition has no data area");
    const uint64_t clusters = (total - data_sector) / cluster_sectors;
    FAIL_IF(clusters == 0, "the partition has no clusters");
    FAIL_IF(clusters >= M_FAT16_CLUSTERS, "FAT32 is not supported");
    const uint64_t used =
        (data_sector + clusters * cluster_sectors) * sector_size;
    FAIL_IF(
        used > disk->start + disk->size - start,
        "the partition reaches past the end of the image");
    volume->clusters = (uint32_t)clusters;
    volume->end = clusters < M_FAT12_CLUSTERS ? M_FAT12_END : M_FAT16_END;
    volume->cluster_size = (uint32_t)cluster_sectors * sector_size;
    volume->root_offset =
        start + (size_t)(reserved + (uint64_t)fats * fat_sectors) * sector_size;
    volume->root_size = (size_t)root_entries * M_ENTRY_SIZE;
    volume->data_offset = start + (size_t)data_sector * sector_size;
    const uint64_t entries = clusters + M_FIRST_CLUSTER;
    const uint64_t fat_bytes =
        volume->end == M_FAT16_END ? entries * 2 : (entries - 1) * 3 / 2 + 2;
    FAIL_IF(
        fat_bytes > (uint64_t)fat_sectors * sector_size,
        "the FAT is too small for the partition");
    return M_ReadFat(
        archive, start + (size_t)reserved * sector_size, (size_t)fat_bytes,
        volume);
}

static bool M_IsCluster(const M_VOLUME *const volume, const uint32_t cluster)
{
    return cluster >= M_FIRST_CLUSTER
        && cluster < volume->clusters + M_FIRST_CLUSTER;
}

// Returns the first count clusters of the chain that starts at first as a
// record. Mem_Free releases it.
static RESULT M_ReadChain(
    const M_VOLUME *const volume, const uint32_t first, const uint32_t count,
    HDI_RECORD **const out, size_t *const out_size)
{
    FAIL_IF(count > volume->clusters, "the cluster chain is too long");
    const size_t size = sizeof(HDI_RECORD) + count * sizeof(uint16_t);
    HDI_RECORD *const record = Mem_Alloc(size);
    record->data_offset = volume->data_offset;
    record->cluster_size = volume->cluster_size;
    record->count = count;
    uint32_t cluster = first;
    for (uint32_t i = 0; i < count; i++) {
        if (!M_IsCluster(volume, cluster)) {
            Mem_Free(record);
            return FAIL("the cluster chain leaves the partition");
        }
        record->clusters[i] = (uint16_t)cluster;
        cluster = volume->next[cluster];
    }
    *out = record;
    *out_size = size;
    return OK;
}

// Reads the directory that starts at cluster first into a new block of
// *out_size bytes. Mem_Free releases it.
static RESULT M_ReadDirectoryData(
    READER *const archive, const M_VOLUME *const volume, const uint32_t first,
    uint8_t **const out, size_t *const out_size)
{
    uint32_t count = 0;
    for (uint32_t cluster = first; cluster < volume->end; count++) {
        FAIL_IF(
            !M_IsCluster(volume, cluster) || volume->seen[cluster],
            "a directory's cluster chain is damaged");
        volume->seen[cluster] = true;
        cluster = volume->next[cluster];
    }
    const size_t size = (size_t)count * volume->cluster_size;
    uint8_t *const data = Mem_Alloc(size);
    uint32_t cluster = first;
    for (uint32_t i = 0; i < count; i++) {
        Reader_Seek(
            archive,
            volume->data_offset
                + (size_t)(cluster - M_FIRST_CLUSTER) * volume->cluster_size);
        Reader_ReadBytes(
            archive, data + (size_t)i * volume->cluster_size,
            volume->cluster_size);
        cluster = volume->next[cluster];
    }
    if (Reader_Failed(archive)) {
        Mem_Free(data);
        return Reader_Result(archive);
    }
    *out = data;
    *out_size = size;
    return OK;
}

// Returns the 8.3 name of a directory entry, decoded from CP932 and in
// lower case. Mem_Free releases it.
static char *M_Name(const uint8_t *const raw)
{
    char bytes[13];
    size_t size = 0;
    for (size_t i = 0; i < 8 && raw[i] != ' '; i++) {
        bytes[size++] = (char)raw[i];
    }
    if (raw[0] == M_ENTRY_KANJI_E5) {
        bytes[0] = (char)M_ENTRY_DELETED;
    }
    if (raw[8] != ' ') {
        bytes[size++] = '.';
        for (size_t i = 8; i < 11 && raw[i] != ' '; i++) {
            bytes[size++] = (char)raw[i];
        }
    }
    bytes[size] = '\0';
    char *name = nullptr;
    if (!Result_Ignore(Cp932_Decode(bytes, size, &name))) {
        name = Str_Dup(bytes);
    }
    Str_ToLower(name);
    return name;
}

static RESULT M_ReadDirectory(
    READER *archive, const M_VOLUME *volume, ARCHIVE_TABLE *table,
    const uint8_t *data, size_t size, const char *prefix, int depth);

static RESULT M_ReadDirectoryEntry(
    READER *const archive, const M_VOLUME *const volume,
    ARCHIVE_TABLE *const table, const uint8_t *const raw,
    const char *const path, const int depth)
{
    const uint32_t first = (uint32_t)(raw[26] | raw[27] << 8);
    if ((raw[11] & M_ATTRIBUTE_DIRECTORY) == 0) {
        const uint32_t size = (uint32_t)raw[28] | (uint32_t)raw[29] << 8
            | (uint32_t)raw[30] << 16 | (uint32_t)raw[31] << 24;
        const uint32_t count =
            (uint32_t)(((uint64_t)size + volume->cluster_size - 1)
                       / volume->cluster_size);
        HDI_RECORD *record = nullptr;
        size_t record_size = 0;
        MUST(
            M_ReadChain(volume, first, count, &record, &record_size), "%s",
            path);
        ArchiveTable_Add(table, path, size, 0, record, record_size);
        Mem_Free(record);
        return OK;
    }
    FAIL_IF(depth >= M_MAX_DEPTH, "%s is nested too deeply", path);
    const HDI_RECORD record = {};
    ArchiveTable_Add(table, path, 0, 0, &record, sizeof(record));
    uint8_t *data = nullptr;
    size_t size = 0;
    MUST(M_ReadDirectoryData(archive, volume, first, &data, &size), "%s", path);
    const RESULT result =
        M_ReadDirectory(archive, volume, table, data, size, path, depth + 1);
    Mem_Free(data);
    return result;
}

static RESULT M_ReadDirectory(
    READER *const archive, const M_VOLUME *const volume,
    ARCHIVE_TABLE *const table, const uint8_t *const data, const size_t size,
    const char *const prefix, const int depth)
{
    for (size_t at = 0; at + M_ENTRY_SIZE <= size; at += M_ENTRY_SIZE) {
        const uint8_t *const raw = data + at;
        if (raw[0] == M_ENTRY_END) {
            break;
        }
        if (raw[0] == M_ENTRY_DELETED || raw[0] == '.' || raw[0] == ' '
            || (raw[11] & M_ATTRIBUTE_VOLUME) != 0) {
            continue;
        }
        char *const name = M_Name(raw);
        char *const path = Str_Format(
            "%s%s%s", prefix, name,
            (raw[11] & M_ATTRIBUTE_DIRECTORY) != 0 ? "/" : "");
        const RESULT result =
            M_ReadDirectoryEntry(archive, volume, table, raw, path, depth);
        Mem_Free(path);
        Mem_Free(name);
        MUST(result);
    }
    return OK;
}

static RESULT M_ReadRoot(
    READER *const archive, const M_VOLUME *const volume,
    ARCHIVE_TABLE *const table)
{
    Reader_Seek(archive, volume->root_offset);
    uint8_t *const root =
        Reader_ReadAlloc(archive, volume->root_size, &g_HeapAllocator);
    MUST(Reader_Result(archive));
    const RESULT result =
        M_ReadDirectory(archive, volume, table, root, volume->root_size, "", 0);
    Allocator_Free(&g_HeapAllocator, root, volume->root_size);
    return result;
}

static RESULT M_ReadTable(READER *const archive, ARCHIVE_TABLE *const table)
{
    M_DISK disk = {};
    MUST(M_ReadDisk(archive, &disk));
    size_t start = 0;
    MUST(M_FindPartition(archive, &disk, &start));
    M_VOLUME volume = {};
    RESULT result = M_ReadVolume(archive, &disk, start, &volume);
    if (IS_OK(result)) {
        result = M_ReadRoot(archive, &volume, table);
    }
    Mem_Free(volume.next);
    Mem_Free(volume.seen);
    return result;
}

static RESULT M_ReadData(
    READER *const archive, const void *const context, const void *const data,
    uint8_t *const out, const size_t size)
{
    const HDI_RECORD *const record = data;
    size_t done = 0;
    for (uint32_t i = 0; i < record->count && done < size; i++) {
        const size_t part = size - done < record->cluster_size
            ? size - done
            : record->cluster_size;
        Reader_Seek(
            archive,
            record->data_offset
                + (size_t)(record->clusters[i] - M_FIRST_CLUSTER)
                    * record->cluster_size);
        Reader_ReadBytes(archive, out + done, part);
        done += part;
    }
    MUST(Reader_Result(archive));
    FAIL_IF(done != size, "its cluster chain is too short");
    return OK;
}

const ARCHIVE_FORMAT g_HdiFormat = {
    .lookup = ARCHIVE_LOOKUP_PATH,
    .read_table = M_ReadTable,
    .read_entry = M_ReadData,
};
