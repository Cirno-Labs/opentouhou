#include "core/archive/zip.h"

#include "core/archive/format.h"
#include "core/archive/deflate.h"
#include "core/base/cp932.h"
#include "core/base/memory.h"
#include "core/hash/crc32.h"
#include "core/io/reader.h"
#include "core/io/writer.h"

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
#define M_VERSION 20
#define M_FLAG_UTF8 0x0800
#define M_DATE_1980 0x0021

typedef struct {
    uint16_t method;
    uint32_t crc;
    uint32_t packed_size;
    uint32_t header_offset;
    DEFLATE_INDEX *index;
    bool verified;
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
    return OK;
}

static RESULT M_ReadEntry(READER *const directory, ARCHIVE_TABLE *const table)
{
    FAIL_IF(
        Reader_ReadU32(directory) != M_ENTRY_SIGNATURE,
        "the central directory is damaged");
    ZIP_RECORD record = {};
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
    char *decoded = nullptr;
    if (IS_OK(result) && (flags & M_FLAG_UTF8) == 0) {
        Result_Ignore(Cp932_Decode(name, name_size, &decoded));
    }
    if (IS_OK(result)) {
        const uint32_t mode =
            made_by >> 8 == M_MADE_ON_UNIX ? attributes >> 16 : 0;
        ArchiveTable_Add(
            table, decoded == nullptr ? name : decoded, size, mode, &record,
            sizeof(record));
    }
    Mem_Free(decoded);
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

static RESULT M_SeekData(READER *const archive, const ZIP_RECORD *const record)
{
    Reader_Seek(archive, record->header_offset);
    const uint32_t signature = Reader_ReadU32(archive);
    Reader_Skip(archive, 22);
    const uint16_t name_size = Reader_ReadU16(archive);
    const uint16_t extra_size = Reader_ReadU16(archive);
    Reader_Skip(archive, (size_t)name_size + extra_size);
    MUST(Reader_Result(archive));
    FAIL_IF(signature != M_HEADER_SIGNATURE, "its local header is damaged");
    FAIL_IF(
        record->packed_size > Reader_Remaining(archive),
        "its packed data is outside the archive");
    return OK;
}

static RESULT M_ReadData(
    READER *const archive, const void *const context, const void *const data,
    uint8_t *const out, const size_t size)
{
    const ZIP_RECORD *const record = data;
    MUST(M_SeekData(archive, record));
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

static RESULT M_OpenEntry(
    READER *const archive, void *const data, const size_t size,
    const ALLOCATOR *const allocator, READER *const out)
{
    ZIP_RECORD *const record = data;
    MUST(M_SeekData(archive, record));
    READER packed = Reader_Slice(archive, record->packed_size);
    MUST(Reader_Result(archive));
    if (record->method == M_METHOD_DEFLATE) {
        return Deflate_Open(
            &packed, size, record->crc, allocator, &record->index, out);
    }
    FAIL_IF(
        record->method != M_METHOD_STORED,
        "its compression method %u is not supported", record->method);
    FAIL_IF(record->packed_size != size, "its stored size does not match");
    if (record->verified) {
        *out = packed;
        return OK;
    }
    uint8_t buffer[READER_BUFFER_SIZE];
    uint32_t crc = 0;
    while (Reader_Remaining(&packed) != 0) {
        const size_t count = Reader_Remaining(&packed) < sizeof(buffer)
            ? Reader_Remaining(&packed)
            : sizeof(buffer);
        Reader_ReadBytes(&packed, buffer, count);
        MUST(Reader_Result(&packed));
        crc = (uint32_t)crc32(crc, buffer, (uInt)count);
    }
    FAIL_IF(crc != record->crc, "its CRC-32 does not match");
    record->verified = true;
    Reader_Seek(&packed, 0);
    *out = packed;
    return OK;
}

static void M_FreeEntry(void *const data)
{
    ZIP_RECORD *const record = data;
    Deflate_FreeIndex(record->index);
}

const ARCHIVE_FORMAT g_ZipFormat = {
    .lookup = ARCHIVE_LOOKUP_PATH,
    .max_entry_size = ZIP_MAX_ENTRY_SIZE,
    .open_entry = M_OpenEntry,
    .free_entry = M_FreeEntry,
    .read_table = M_ReadTable,
    .read_entry = M_ReadData,
};

typedef struct {
    uint32_t crc;
    uint32_t packed_size;
    uint32_t offset;
} M_WRITTEN;

static void M_WriteHeader(
    WRITER *const out, const ZIP_ENTRY *const entry,
    const M_WRITTEN *const written)
{
    Writer_WriteU16(out, M_VERSION);
    Writer_WriteU16(out, M_FLAG_UTF8);
    Writer_WriteU16(out, M_METHOD_DEFLATE);
    Writer_WriteU16(out, 0);
    Writer_WriteU16(out, M_DATE_1980);
    Writer_WriteU32(out, written->crc);
    Writer_WriteU32(out, written->packed_size);
    Writer_WriteU32(out, (uint32_t)entry->size);
    Writer_WriteU16(out, (uint16_t)strlen(entry->name));
    Writer_WriteU16(out, 0);
}

static RESULT M_WriteEntry(
    WRITER *const out, const ZIP_ENTRY *const entry, M_WRITTEN *const written)
{
    FAIL_IF(
        entry->size > UINT32_MAX / 2 || strlen(entry->name) > UINT16_MAX,
        "%s is too large for a zip", entry->name);
    z_stream stream = {};
    FAIL_IF(
        deflateInit2(
            &stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 9,
            Z_DEFAULT_STRATEGY)
            != Z_OK,
        "cannot start deflate");
    const uLong bound = deflateBound(&stream, (uLong)entry->size);
    uint8_t *const packed = Mem_Alloc(bound);
    stream.next_in = (Bytef *)entry->data;
    stream.avail_in = (uInt)entry->size;
    stream.next_out = packed;
    stream.avail_out = (uInt)bound;
    const int status = deflate(&stream, Z_FINISH);
    deflateEnd(&stream);
    if (status != Z_STREAM_END) {
        Mem_Free(packed);
        return FAIL("cannot deflate %s", entry->name);
    }
    *written = (M_WRITTEN) {
        .crc = Crc32_Compute(entry->data, entry->size),
        .packed_size = (uint32_t)stream.total_out,
        .offset = (uint32_t)Writer_Tell(out),
    };
    Writer_WriteU32(out, M_HEADER_SIGNATURE);
    M_WriteHeader(out, entry, written);
    Writer_WriteBytes(out, entry->name, strlen(entry->name));
    Writer_WriteBytes(out, packed, written->packed_size);
    Mem_Free(packed);
    return OK;
}

RESULT Zip_Write(
    WRITER *const out, const ZIP_ENTRY *const entries, const size_t count)
{
    FAIL_IF(count > UINT16_MAX, "too many entries for a zip");
    M_WRITTEN *const written = Mem_AllocArrZ(count, sizeof(*written));
    RESULT result = OK;
    for (size_t i = 0; i < count && IS_OK(result); i++) {
        result = M_WriteEntry(out, &entries[i], &written[i]);
    }
    if (!IS_OK(result)) {
        Mem_Free(written);
        return result;
    }
    const size_t directory = Writer_Tell(out);
    for (size_t i = 0; i < count; i++) {
        Writer_WriteU32(out, M_ENTRY_SIGNATURE);
        Writer_WriteU16(out, M_VERSION);
        M_WriteHeader(out, &entries[i], &written[i]);
        Writer_WriteU16(out, 0);
        Writer_WriteU16(out, 0);
        Writer_WriteU16(out, 0);
        Writer_WriteU32(out, 0);
        Writer_WriteU32(out, written[i].offset);
        Writer_WriteBytes(out, entries[i].name, strlen(entries[i].name));
    }
    const size_t end = Writer_Tell(out);
    Writer_WriteU32(out, M_END_SIGNATURE);
    Writer_WriteU16(out, 0);
    Writer_WriteU16(out, 0);
    Writer_WriteU16(out, (uint16_t)count);
    Writer_WriteU16(out, (uint16_t)count);
    Writer_WriteU32(out, (uint32_t)(end - directory));
    Writer_WriteU32(out, (uint32_t)directory);
    Writer_WriteU16(out, 0);
    Mem_Free(written);
    return Writer_Result(out);
}
