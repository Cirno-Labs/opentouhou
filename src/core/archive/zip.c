#include "core/archive/zip.h"

#include "core/archive/format.h"
#include "core/base/memory.h"
#include "core/hash/crc32.h"
#include "core/io/reader.h"

#include <string.h>
#include <zlib.h>

#define M_END_SIGNATURE 0x06054B50
#define M_ENTRY_SIGNATURE 0x02014B50
#define M_HEADER_SIGNATURE 0x04034B50
#define M_END_SIZE 22
#define M_MAX_COMMENT 0xFFFF
#define M_METHOD_STORED 0
#define M_METHOD_DEFLATE 8
#define M_FLAG_ENCRYPTED 0x0001
#define M_MADE_ON_UNIX 3

typedef struct {
    uint16_t method;
    uint32_t crc;
    uint32_t packed_size;
    uint32_t header_offset;
} ZIP_RECORD;

// Finds the end of central directory record, which lies in the last
// M_END_SIZE + M_MAX_COMMENT bytes, and returns its position.
static RESULT M_FindEnd(READER *const archive, size_t *const out)
{
    const size_t size = Reader_Size(archive);
    FAIL_IF(size < M_END_SIZE, "the archive is too short");
    const size_t lowest = size - M_END_SIZE > M_MAX_COMMENT
        ? size - M_END_SIZE - M_MAX_COMMENT
        : 0;
    Reader_Seek(archive, lowest);
    uint8_t *const tail =
        Reader_ReadAlloc(archive, size - lowest, &g_HeapAllocator);
    MUST(Reader_Result(archive));
    bool found = false;
    for (size_t position = size - lowest - M_END_SIZE + 1; position-- > 0;) {
        const uint32_t signature = (uint32_t)tail[position]
            | (uint32_t)tail[position + 1] << 8
            | (uint32_t)tail[position + 2] << 16
            | (uint32_t)tail[position + 3] << 24;
        if (signature == M_END_SIGNATURE) {
            *out = lowest + position;
            found = true;
            break;
        }
    }
    Allocator_Free(&g_HeapAllocator, tail, size - lowest);
    FAIL_IF(!found, "the archive has no end of central directory");
    return OK;
}

static RESULT M_CheckEntry(
    const char *const name, const size_t name_size, const uint16_t flags,
    const ZIP_RECORD *const record, const uint32_t size)
{
    FAIL_IF(strlen(name) != name_size, "the name of an entry has a NUL byte");
    FAIL_IF(
        (flags & M_FLAG_ENCRYPTED) != 0,
        "%s is encrypted, which is not supported", name);
    FAIL_IF(
        record->packed_size == UINT32_MAX || size == UINT32_MAX
            || record->header_offset == UINT32_MAX,
        "%s needs ZIP64, which is not supported", name);
    FAIL_IF(size > ZIP_MAX_ENTRY_SIZE, "%s is too large to unpack", name);
    return OK;
}

static RESULT M_ReadEntry(READER *const directory, ARCHIVE_TABLE *const table)
{
    FAIL_IF(
        Reader_ReadU32(directory) != M_ENTRY_SIGNATURE,
        "the central directory is damaged");
    ZIP_RECORD record;
    const uint16_t made_by = Reader_ReadU16(directory);
    Reader_Skip(directory, 2);
    const uint16_t flags = Reader_ReadU16(directory);
    record.method = Reader_ReadU16(directory);
    Reader_Skip(directory, 4);
    record.crc = Reader_ReadU32(directory);
    record.packed_size = Reader_ReadU32(directory);
    const uint32_t size = Reader_ReadU32(directory);
    const uint16_t name_size = Reader_ReadU16(directory);
    const uint16_t extra_size = Reader_ReadU16(directory);
    const uint16_t comment_size = Reader_ReadU16(directory);
    Reader_Skip(directory, 4);
    const uint32_t attributes = Reader_ReadU32(directory);
    record.header_offset = Reader_ReadU32(directory);
    char *const name = Mem_Alloc((size_t)name_size + 1);
    Reader_ReadBytes(directory, name, name_size);
    name[name_size] = '\0';
    Reader_Skip(directory, (size_t)extra_size + comment_size);
    RESULT result = Reader_Result(directory);
    if (IS_OK(result)) {
        result = M_CheckEntry(name, name_size, flags, &record, size);
    }
    if (IS_OK(result)) {
        const uint32_t mode =
            made_by >> 8 == M_MADE_ON_UNIX ? attributes >> 16 : 0;
        ArchiveTable_Add(table, name, size, mode, &record, sizeof(record));
    }
    Mem_Free(name);
    return result;
}

static RESULT M_ReadTable(READER *const archive, ARCHIVE_TABLE *const table)
{
    size_t end = 0;
    MUST(M_FindEnd(archive, &end));
    Reader_Seek(archive, end + 4);
    const uint16_t disk = Reader_ReadU16(archive);
    const uint16_t directory_disk = Reader_ReadU16(archive);
    const uint16_t disk_entries = Reader_ReadU16(archive);
    const uint16_t entries = Reader_ReadU16(archive);
    const uint32_t directory_size = Reader_ReadU32(archive);
    const uint32_t directory_offset = Reader_ReadU32(archive);
    MUST(Reader_Result(archive));
    FAIL_IF(
        disk != 0 || directory_disk != 0 || disk_entries != entries,
        "archives of several parts are not supported");
    FAIL_IF(
        entries == UINT16_MAX || directory_offset == UINT32_MAX,
        "the archive needs ZIP64, which is not supported");
    FAIL_IF(
        directory_offset > end || directory_size > end - directory_offset,
        "the central directory is outside the archive");
    Reader_Seek(archive, directory_offset);
    READER directory = Reader_Slice(archive, directory_size);
    for (uint16_t i = 0; i < entries; i++) {
        MUST(M_ReadEntry(&directory, table));
    }
    return OK;
}

static RESULT M_Inflate(
    const uint8_t *const packed, const size_t packed_size, uint8_t *const out,
    const size_t size)
{
    z_stream stream = {};
    FAIL_IF(
        inflateInit2(&stream, -MAX_WBITS) != Z_OK, "cannot start to unpack: %s",
        stream.msg != nullptr ? stream.msg : "zlib failed");
    stream.next_in = (Bytef *)packed;
    stream.avail_in = (uInt)packed_size;
    stream.next_out = out;
    stream.avail_out = (uInt)size;
    const int status = inflate(&stream, Z_FINISH);
    const bool whole = status == Z_STREAM_END && stream.avail_out == 0;
    const RESULT result = whole
        ? OK
        : FAIL(
              "the packed data is damaged: %s",
              stream.msg != nullptr ? stream.msg : "wrong size");
    inflateEnd(&stream);
    return result;
}

static RESULT M_ReadDeflated(
    READER *const archive, const ZIP_RECORD *const record, uint8_t *const out,
    const size_t size)
{
    uint8_t *const packed =
        Reader_ReadAlloc(archive, record->packed_size, &g_HeapAllocator);
    MUST(Reader_Result(archive));
    const RESULT result = M_Inflate(packed, record->packed_size, out, size);
    Allocator_Free(&g_HeapAllocator, packed, record->packed_size);
    return result;
}

static RESULT M_ReadData(
    READER *const archive, const void *const data, uint8_t *const out,
    const size_t size)
{
    const ZIP_RECORD *const record = data;
    Reader_Seek(archive, record->header_offset);
    const uint32_t signature = Reader_ReadU32(archive);
    Reader_Skip(archive, 22);
    const uint16_t name_size = Reader_ReadU16(archive);
    const uint16_t extra_size = Reader_ReadU16(archive);
    Reader_Skip(archive, (size_t)name_size + extra_size);
    MUST(Reader_Result(archive));
    FAIL_IF(signature != M_HEADER_SIGNATURE, "its local header is damaged");
    if (record->method == M_METHOD_STORED) {
        FAIL_IF(record->packed_size != size, "its stored size does not match");
        Reader_ReadBytes(archive, out, size);
        MUST(Reader_Result(archive));
    } else if (record->method == M_METHOD_DEFLATE) {
        MUST(M_ReadDeflated(archive, record, out, size));
    } else {
        return FAIL(
            "its compression method %u is not supported", record->method);
    }
    FAIL_IF(
        Crc32_Compute(out, size) != record->crc, "its CRC-32 does not match");
    return OK;
}

const ARCHIVE_FORMAT g_ZipFormat = {
    .lookup = ARCHIVE_LOOKUP_PATH,
    .read_table = M_ReadTable,
    .read_entry = M_ReadData,
};
