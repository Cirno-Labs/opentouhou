#include "pc98/zun/pf.h"

#include "core/archive/format.h"
#include "core/base/cp932.h"

#include <string.h>

#define M_ENTRY_COUNT 254
#define M_NAME_SIZE 13
#define M_UTF8_NAME_SIZE 37
#define M_RAW 0xf388
#define M_RLE 0x9595

typedef struct {
    uint32_t offset, packed_size;
    bool compressed;
} PF_RECORD;

typedef struct {
    char name[M_UTF8_NAME_SIZE];
    uint32_t decoded_size;
    PF_RECORD record;
} PF_ENTRY;

static const uint8_t m_Key76 = 0x76;
static const uint8_t m_Key12 = 0x12;

static RESULT M_ReadName(READER *const reader, char out[M_UTF8_NAME_SIZE])
{
    char bytes[M_NAME_SIZE];
    Reader_ReadBytes(reader, bytes, sizeof(bytes));
    MUST(Reader_Result(reader));
    size_t length = 0;
    while (length < sizeof(bytes) && bytes[length] != '\0') {
        bytes[length] = (char)~(uint8_t)bytes[length];
        FAIL_IF(bytes[length] == '\0', "invalid entry name");
        length++;
    }
    FAIL_IF(length == 0 || length == sizeof(bytes), "invalid entry name");
    char *decoded;
    MUST(Cp932_Decode(bytes, length, &decoded));
    const size_t size = strlen(decoded) + 1;
    if (size > M_UTF8_NAME_SIZE) {
        Mem_Free(decoded);
        return FAIL("entry name is too long");
    }
    memcpy(out, decoded, size);
    Mem_Free(decoded);
    for (char *c = out; *c != '\0'; c++) {
        if (*c >= 'A' && *c <= 'Z') {
            *c = (char)(*c - 'A' + 'a');
        }
    }
    FAIL_IF(
        strchr(out, '/') != nullptr || strchr(out, '\\') != nullptr
            || strchr(out, ':') != nullptr || strcmp(out, ".") == 0
            || strcmp(out, "..") == 0,
        "invalid entry path");
    return OK;
}

static RESULT M_ReadTable(READER *const archive, ARCHIVE_TABLE *const table)
{
    PF_ENTRY entries[M_ENTRY_COUNT];
    size_t count = 0;
    for (; count < M_ENTRY_COUNT; count++) {
        const uint16_t type = Reader_ReadU16(archive);
        MUST(Reader_Result(archive));
        if (type == 0) {
            Reader_Skip(archive, 30);
            MUST(Reader_Result(archive));
            break;
        }
        FAIL_IF(type != M_RAW && type != M_RLE, "unknown entry type");
        Reader_Skip(archive, 1);
        PF_ENTRY *const entry = &entries[count];
        MUST(M_ReadName(archive, entry->name));
        entry->record.packed_size = Reader_ReadU32(archive);
        entry->decoded_size = Reader_ReadU32(archive);
        entry->record.offset = Reader_ReadU32(archive);
        Reader_Skip(archive, 4);
        MUST(Reader_Result(archive));
        entry->record.compressed = type == M_RLE;
        FAIL_IF(
            entry->record.packed_size > UINT16_MAX
                || entry->decoded_size > UINT16_MAX,
            "%s: entry exceeds 16-bit byte counts", entry->name);
        FAIL_IF(
            entry->record.offset > Reader_Size(archive)
                || entry->record.packed_size
                    > Reader_Size(archive) - entry->record.offset,
            "%s: entry range is outside the archive", entry->name);
        FAIL_IF(
            !entry->record.compressed
                && entry->decoded_size != entry->record.packed_size,
            "%s: uncompressed sizes differ", entry->name);
        FAIL_IF(
            entry->record.compressed
                && (uint64_t)entry->decoded_size
                    > (uint64_t)entry->record.packed_size * 128,
            "%s: decoded size exceeds the RLE limit", entry->name);
    }
    for (size_t i = 0; i < count; i++) {
        FAIL_IF(
            entries[i].record.offset < Reader_Tell(archive),
            "%s: entry overlaps the directory", entries[i].name);
    }
    for (size_t i = 0; i < count; i++) {
        bool duplicate = false;
        for (size_t j = 0; j < i; j++) {
            if (strcmp(entries[i].name, entries[j].name) == 0) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            ArchiveTable_Add(
                table, entries[i].name, entries[i].decoded_size, 0,
                &entries[i].record, sizeof(entries[i].record));
        }
    }
    return OK;
}

static RESULT M_ReadEntry(
    READER *const archive, const void *const context, const void *const data,
    uint8_t *const out, const size_t size)
{
    const uint8_t key = *(const uint8_t *)context;
    const PF_RECORD *const record = data;
    Reader_Seek(archive, record->offset);
    READER input = Reader_Slice(archive, record->packed_size);
    MUST(Reader_Result(archive));
    if (!record->compressed) {
        Reader_ReadBytes(&input, out, size);
        MUST(Reader_Result(&input));
        for (size_t i = 0; i < size; i++) {
            out[i] ^= key;
        }
        return OK;
    }
    int32_t previous = -1;
    uint8_t repeats = 0;
    for (size_t i = 0; i < size; i++) {
        if (repeats != 0) {
            out[i] = (uint8_t)previous;
            repeats--;
            continue;
        }
        const uint8_t value = Reader_ReadU8(&input) ^ key;
        MUST(Reader_Result(&input));
        out[i] = value;
        // A repeated byte at the end of the packed range has no count.
        if (value == previous && Reader_Remaining(&input) != 0) {
            repeats = Reader_ReadU8(&input) ^ key;
            MUST(Reader_Result(&input));
        }
        previous = value;
    }
    return OK;
}

static const ARCHIVE_FORMAT m_Formats[] = {
    [PF_TH01] = {
        .lookup = ARCHIVE_LOOKUP_PATH,
        .read_table = M_ReadTable,
        .read_entry = M_ReadEntry,
        .context = &m_Key76,
    },
    [PF_KEY_12] = {
        .lookup = ARCHIVE_LOOKUP_PATH,
        .read_table = M_ReadTable,
        .read_entry = M_ReadEntry,
        .context = &m_Key12,
    },
};

RESULT Pf_Open(
    READER *const archive, const PF_GAME game, const char *const name,
    const ALLOCATOR *const allocator, VFS_SOURCE **const out)
{
    *out = nullptr;
    ARCHIVE *decoded;
    MUST(
        Archive_Open(&m_Formats[game], archive, allocator, &decoded), "%s",
        name);
    *out = Archive_ToSource(decoded, "");
    return OK;
}
