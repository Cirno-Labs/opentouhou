#include "stg/pbg4.h"

#include "core/archive/format.h"
#include "core/base/array.h"
#include "core/base/cp932.h"
#include "core/base/memory.h"

#include "stg/lzss.h"

#include <string.h>

#define M_MAGIC 0x34474250u
// Bounds the unpacked bytes per packed byte: a match is 18 bits and unpacks
// to at most 18 bytes.
#define M_MAX_UNPACKED_PER_PACKED 8

// Holds where the packed data of an entry lies.
typedef struct {
    uint32_t data_offset;
    uint32_t data_size;
} PBG4_RECORD;

typedef struct {
    char *name;
    uint32_t decoded_size;
    PBG4_RECORD record;
} PBG4_ENTRY;

typedef ARRAY(PBG4_ENTRY) PBG4_ENTRY_ARRAY;

static RESULT M_ReadName(READER *const table, char **const out)
{
    const size_t start = Reader_Tell(table);
    const size_t remaining = Reader_Remaining(table);
    const uint8_t *const rest = Reader_Borrow(table, remaining);
    const uint8_t *const end =
        rest == nullptr ? nullptr : memchr(rest, 0, remaining);
    FAIL_IF(end == nullptr, "table runs past its unpacked size");
    const size_t length = (size_t)(end - rest);
    Reader_Seek(table, start + length + 1);
    return Cp932_Decode((const char *)rest, length, out);
}

static RESULT M_ReadEntries(
    READER *const table, const uint32_t entry_count,
    const uint32_t table_offset, PBG4_ENTRY_ARRAY *const entries)
{
    for (uint32_t i = 0; i < entry_count; i++) {
        char *name;
        MUST(M_ReadName(table, &name));
        uint32_t data_offset;
        uint32_t decoded_size;
        Reader_Read(table, &data_offset);
        Reader_Read(table, &decoded_size);
        // Skips the checksum, which the original never checks.
        Reader_Skip(table, 4);
        if (Reader_Failed(table)) {
            Mem_Free(name);
            return FAIL("table runs past its unpacked size");
        }

        PBG4_ENTRY *const entry = Array_AddOne(entries);
        entry->name = name;
        entry->decoded_size = decoded_size;
        entry->record = (PBG4_RECORD) { .data_offset = data_offset };
    }

    for (int32_t i = 0; i < entries->count; i++) {
        PBG4_RECORD *const record = &entries->items[i].record;
        const uint32_t end = i + 1 < entries->count
            ? entries->items[i + 1].record.data_offset
            : table_offset;
        FAIL_IF(
            end < record->data_offset, "%s: entry data ends before it starts",
            entries->items[i].name);
        record->data_size = end - record->data_offset;
    }
    return OK;
}

static RESULT M_UnpackTable(
    READER *const archive, const uint32_t table_offset,
    const uint32_t table_size, uint8_t *const out)
{
    Reader_Seek(archive, table_offset);
    READER packed = Reader_Slice(archive, Reader_Remaining(archive));
    size_t decoded;
    FAIL_IF(
        !Result_Ignore(
            Lzss_Decode(&packed, out, table_size, &decoded, nullptr)),
        "table is longer than recorded");
    FAIL_IF(decoded != table_size, "table is shorter than recorded");
    return OK;
}

static RESULT M_ReadTable(READER *const archive, ARCHIVE_TABLE *const table)
{
    uint32_t magic;
    uint32_t entry_count;
    uint32_t table_offset;
    uint32_t table_size;
    Reader_Read(archive, &magic);
    FAIL_IF(magic != M_MAGIC || Reader_Failed(archive), "bad magic");
    Reader_Read(archive, &entry_count);
    Reader_Read(archive, &table_offset);
    Reader_Read(archive, &table_size);
    FAIL_IF(Reader_Failed(archive), "header runs past the end");
    // The original reads the count as signed.
    FAIL_IF((int32_t)entry_count <= 0, "archive has no entries");
    FAIL_IF(
        table_offset >= Reader_Size(archive),
        "table runs past the end of the archive");
    const uint64_t packed_size = Reader_Size(archive) - table_offset;
    FAIL_IF(
        table_size > packed_size * M_MAX_UNPACKED_PER_PACKED,
        "table is longer than its packed data can hold");

    uint8_t *const unpacked = Mem_Alloc(table_size);
    PBG4_ENTRY_ARRAY entries;
    Array_Init(&entries, &g_HeapAllocator);
    RESULT result = M_UnpackTable(archive, table_offset, table_size, unpacked);
    if (IS_OK(result)) {
        READER reader;
        Reader_OpenBuffer(&reader, unpacked, table_size, nullptr);
        result = M_ReadEntries(&reader, entry_count, table_offset, &entries);
    }
    for (int32_t i = 0; i < entries.count; i++) {
        const PBG4_ENTRY *const entry = &entries.items[i];
        if (IS_OK(result)) {
            ArchiveTable_Add(
                table, entry->name, entry->decoded_size, 0, &entry->record,
                sizeof(entry->record));
        }
        Mem_Free(entry->name);
    }
    Array_Free(&entries);
    Mem_Free(unpacked);
    return result;
}

static RESULT M_ReadEntry(
    READER *const archive, const void *const data, uint8_t *const out,
    const size_t size)
{
    const PBG4_RECORD *const record = data;
    Reader_Seek(archive, record->data_offset);
    READER slice = Reader_Slice(archive, record->data_size);
    size_t decoded;
    FAIL_IF(
        !Result_Ignore(Lzss_Decode(&slice, out, size, &decoded, nullptr)),
        "decoded data is longer than recorded");
    FAIL_IF(decoded != size, "decoded data is shorter than recorded");
    return OK;
}

const ARCHIVE_FORMAT g_Pbg4Format = {
    .lookup = ARCHIVE_LOOKUP_LAST_NOCASE,
    .read_table = M_ReadTable,
    .read_entry = M_ReadEntry,
};
