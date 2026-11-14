#include "stg/pbg3.h"

#include "core/archive/format.h"
#include "core/base/array.h"
#include "core/base/cp932.h"
#include "core/base/memory.h"

#define M_MAGIC 0x33474250u
#define M_MAX_NAME_LENGTH 255
#define M_DICT_SIZE 0x2000u

// Holds where the packed data of an entry lies, and its check value.
typedef struct {
    uint32_t checksum;
    uint32_t data_offset;
    uint32_t data_size;
} PBG3_RECORD;

typedef struct {
    char *name;
    uint32_t decoded_size;
    PBG3_RECORD record;
} PBG3_ENTRY;

typedef ARRAY(PBG3_ENTRY) PBG3_ENTRY_ARRAY;

// Reads bits most significant bit first out of a READER, and keeps the
// wrapping sum of every byte it fetches from the reader, for the entry
// decoder's checksum.
typedef struct {
    READER *reader;
    uint32_t byte;
    int32_t bits_left;
    uint32_t checksum;
} BIT_READER;

static void M_BitReader_Init(BIT_READER *const br, READER *const reader)
{
    br->reader = reader;
    br->byte = 0;
    br->bits_left = 0;
    br->checksum = 0;
}

static uint32_t M_ReadBits(BIT_READER *const br, const int32_t count)
{
    uint32_t value = 0;
    for (int32_t i = 0; i < count; i++) {
        if (br->bits_left == 0) {
            br->byte = Reader_ReadU8(br->reader);
            br->checksum += br->byte;
            br->bits_left = 8;
        }
        br->bits_left--;
        value = (value << 1) | ((br->byte >> br->bits_left) & 1);
    }
    return value;
}

// Reads a varint: 2 bits giving the byte count minus 1, then that many
// bytes, most significant bit first.
static uint32_t M_ReadVarint(BIT_READER *const br)
{
    const uint32_t byte_count = M_ReadBits(br, 2) + 1;
    return M_ReadBits(br, (int32_t)byte_count * 8);
}

static RESULT M_ReadName(BIT_READER *const br, char **const out)
{
    uint8_t bytes[M_MAX_NAME_LENGTH];
    int32_t length = 0;
    for (;;) {
        const uint8_t byte = (uint8_t)M_ReadBits(br, 8);
        if (byte == 0) {
            break;
        }
        FAIL_IF(length >= M_MAX_NAME_LENGTH, "entry name is too long");
        bytes[length++] = byte;
    }
    return Cp932_Decode((const char *)bytes, (size_t)length, out);
}

static RESULT M_ReadEntries(
    READER *const archive, const uint32_t entry_count,
    const uint32_t table_offset, PBG3_ENTRY_ARRAY *const entries)
{
    Reader_Seek(archive, table_offset);
    BIT_READER br;
    M_BitReader_Init(&br, archive);

    for (uint32_t i = 0; i < entry_count; i++) {
        M_ReadVarint(&br); // unk1, unused
        M_ReadVarint(&br); // unk0, unused
        const uint32_t checksum = M_ReadVarint(&br);
        const uint32_t data_offset = M_ReadVarint(&br);
        const uint32_t decoded_size = M_ReadVarint(&br);
        char *name;
        MUST(M_ReadName(&br, &name));
        if (Reader_Failed(archive)) {
            Mem_Free(name);
            return FAIL("table runs past the end of the archive");
        }

        PBG3_ENTRY *const entry = Array_AddOne(entries);
        entry->name = name;
        entry->decoded_size = decoded_size;
        entry->record = (PBG3_RECORD) {
            .checksum = checksum,
            .data_offset = data_offset,
        };
    }

    for (int32_t i = 0; i < entries->count; i++) {
        PBG3_RECORD *const record = &entries->items[i].record;
        const uint32_t end = i + 1 < entries->count
            ? entries->items[i + 1].record.data_offset
            : table_offset;
        FAIL_IF(
            end < record->data_offset, "%s: entry data overlaps the table",
            entries->items[i].name);
        record->data_size = end - record->data_offset;
    }
    return OK;
}

static RESULT M_ReadTable(READER *const archive, ARCHIVE_TABLE *const table)
{
    uint32_t magic;
    Reader_Read(archive, &magic);
    FAIL_IF(magic != M_MAGIC || Reader_Failed(archive), "bad magic");

    BIT_READER br;
    M_BitReader_Init(&br, archive);
    const uint32_t entry_count = M_ReadVarint(&br);
    const uint32_t table_offset = M_ReadVarint(&br);
    FAIL_IF(Reader_Failed(archive), "header runs past the end");

    PBG3_ENTRY_ARRAY entries;
    Array_Init(&entries, &g_HeapAllocator);
    const RESULT result =
        M_ReadEntries(archive, entry_count, table_offset, &entries);
    for (int32_t i = 0; i < entries.count; i++) {
        const PBG3_ENTRY *const entry = &entries.items[i];
        if (IS_OK(result)) {
            ArchiveTable_Add(
                table, entry->name, entry->decoded_size, 0, &entry->record,
                sizeof(entry->record));
        }
        Mem_Free(entry->name);
    }
    Array_Free(&entries);
    return result;
}

// Decompresses the LZSS stream of one entry's data into out, which has
// room for exactly size bytes, the entry's unpacked size.
static RESULT M_ReadEntry(
    READER *const archive, const void *const data, uint8_t *const out,
    const size_t size)
{
    const PBG3_RECORD *const record = data;
    Reader_Seek(archive, record->data_offset);
    READER slice = Reader_Slice(archive, record->data_size);
    BIT_READER br;
    M_BitReader_Init(&br, &slice);

    uint8_t dict[M_DICT_SIZE] = { 0 };
    uint32_t dict_pos = 1;
    size_t decoded_count = 0;

    while (decoded_count < size) {
        if (M_ReadBits(&br, 1) != 0) {
            const uint8_t byte = (uint8_t)M_ReadBits(&br, 8);
            out[decoded_count++] = byte;
            dict[dict_pos] = byte;
            dict_pos = (dict_pos + 1) & (M_DICT_SIZE - 1);
            continue;
        }
        const uint32_t offset = M_ReadBits(&br, 13);
        if (offset == 0) {
            break;
        }
        const uint32_t length = M_ReadBits(&br, 4) + 3;
        for (uint32_t i = 0; i < length; i++) {
            FAIL_IF(
                decoded_count >= size, "decoded data is longer than recorded");
            const uint8_t byte = dict[(offset + i) & (M_DICT_SIZE - 1)];
            out[decoded_count++] = byte;
            dict[dict_pos] = byte;
            dict_pos = (dict_pos + 1) & (M_DICT_SIZE - 1);
        }
    }

    FAIL_IF(decoded_count != size, "decoded data is shorter than recorded");
    FAIL_IF(br.checksum != record->checksum, "checksum mismatch");
    return OK;
}

const ARCHIVE_FORMAT g_Pbg3Format = {
    .lookup = ARCHIVE_LOOKUP_LAST_NOCASE,
    .read_table = M_ReadTable,
    .read_entry = M_ReadEntry,
};
