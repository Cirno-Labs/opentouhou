// SPDX-License-Identifier: GPL-3.0-only

#include "othpatch/zip.h"

#include "core/base/crc32.h"
#include "core/base/memory.h"
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

static RESULT M_FindEnd(
    const uint8_t *const data, const size_t size, size_t *const out)
{
    FAIL_IF(size < M_END_SIZE, "the archive is too short");
    const size_t lowest = size - M_END_SIZE > M_MAX_COMMENT
        ? size - M_END_SIZE - M_MAX_COMMENT
        : 0;
    for (size_t position = size - M_END_SIZE + 1; position-- > lowest;) {
        const uint32_t signature = (uint32_t)data[position]
            | (uint32_t)data[position + 1] << 8
            | (uint32_t)data[position + 2] << 16
            | (uint32_t)data[position + 3] << 24;
        if (signature == M_END_SIGNATURE) {
            *out = position;
            return OK;
        }
    }
    return FAIL("the archive has no end of central directory");
}

static RESULT M_ReadEntry(
    READER *const reader, ARENA *const arena, ZIP_ENTRY *const entry)
{
    FAIL_IF(
        Reader_ReadU32(reader) != M_ENTRY_SIGNATURE,
        "the central directory is damaged");
    const uint16_t made_by = Reader_ReadU16(reader);
    Reader_Skip(reader, 2);
    const uint16_t flags = Reader_ReadU16(reader);
    entry->method = Reader_ReadU16(reader);
    Reader_Skip(reader, 4);
    entry->crc = Reader_ReadU32(reader);
    entry->packed_size = Reader_ReadU32(reader);
    entry->size = Reader_ReadU32(reader);
    const uint16_t name_size = Reader_ReadU16(reader);
    const uint16_t extra_size = Reader_ReadU16(reader);
    const uint16_t comment_size = Reader_ReadU16(reader);
    Reader_Skip(reader, 4);
    const uint32_t attributes = Reader_ReadU32(reader);
    entry->header_offset = Reader_ReadU32(reader);
    char *const name = Arena_Alloc(arena, (size_t)name_size + 1);
    Reader_ReadBytes(reader, name, name_size);
    name[name_size] = '\0';
    Reader_Skip(reader, (size_t)extra_size + comment_size);
    MUST(Reader_Result(reader));
    entry->name = name;
    entry->mode = made_by >> 8 == M_MADE_ON_UNIX ? attributes >> 16 : 0;
    FAIL_IF(strlen(name) != name_size, "the name of an entry has a NUL byte");
    FAIL_IF(
        (flags & M_FLAG_ENCRYPTED) != 0,
        "%s is encrypted, which is not "
        "supported",
        name);
    FAIL_IF(
        entry->packed_size == UINT32_MAX || entry->size == UINT32_MAX
            || entry->header_offset == UINT32_MAX,
        "%s needs ZIP64, which is not supported", name);
    return OK;
}

RESULT Zip_List(
    const uint8_t *const data, const size_t size, ARENA *const arena,
    ZIP_ENTRY_LIST *const out)
{
    Array_Init(out, Arena_Allocator(arena));
    size_t end = 0;
    MUST(M_FindEnd(data, size, &end));
    READER reader;
    Reader_OpenBuffer(&reader, data, size);
    Reader_Seek(&reader, end + 4);
    const uint16_t disk = Reader_ReadU16(&reader);
    const uint16_t directory_disk = Reader_ReadU16(&reader);
    const uint16_t disk_entries = Reader_ReadU16(&reader);
    const uint16_t entries = Reader_ReadU16(&reader);
    const uint32_t directory_size = Reader_ReadU32(&reader);
    const uint32_t directory_offset = Reader_ReadU32(&reader);
    MUST(Reader_Result(&reader));
    FAIL_IF(
        disk != 0 || directory_disk != 0 || disk_entries != entries,
        "archives of several parts are not supported");
    FAIL_IF(
        entries == UINT16_MAX || directory_offset == UINT32_MAX,
        "the archive needs ZIP64, which is not supported");
    FAIL_IF(
        directory_offset > end || directory_size > end - directory_offset,
        "the central directory is outside the archive");
    Reader_Seek(&reader, directory_offset);
    READER directory = Reader_Slice(&reader, directory_size);
    for (uint16_t i = 0; i < entries; i++) {
        ZIP_ENTRY entry;
        MUST(M_ReadEntry(&directory, arena, &entry));
        Array_Append(out, entry);
    }
    return OK;
}

const ZIP_ENTRY *Zip_Find(
    const ZIP_ENTRY_LIST *const list, const char *const name)
{
    for (int32_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].name, name) == 0) {
            return &list->items[i];
        }
    }
    return nullptr;
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

static RESULT M_Unpack(
    const uint8_t *const data, const size_t size, const ZIP_ENTRY *const entry,
    uint8_t *const out)
{
    READER reader;
    Reader_OpenBuffer(&reader, data, size);
    Reader_Seek(&reader, entry->header_offset);
    const uint32_t signature = Reader_ReadU32(&reader);
    Reader_Skip(&reader, 22);
    const uint16_t name_size = Reader_ReadU16(&reader);
    const uint16_t extra_size = Reader_ReadU16(&reader);
    Reader_Skip(&reader, (size_t)name_size + extra_size);
    const uint8_t *const packed = Reader_Borrow(&reader, entry->packed_size);
    MUST(Reader_Result(&reader));
    FAIL_IF(signature != M_HEADER_SIGNATURE, "its local header is damaged");
    if (entry->method == M_METHOD_STORED) {
        FAIL_IF(
            entry->packed_size != entry->size,
            "its stored size does not match");
        if (entry->size > 0) {
            memcpy(out, packed, entry->size);
        }
    } else if (entry->method == M_METHOD_DEFLATE) {
        MUST(M_Inflate(packed, entry->packed_size, out, entry->size));
    } else {
        return FAIL(
            "its compression method %u is not supported", entry->method);
    }
    FAIL_IF(
        Crc32_Compute(out, entry->size) != entry->crc,
        "its CRC-32 does not match");
    return OK;
}

RESULT Zip_Read(
    const uint8_t *const data, const size_t size, const ZIP_ENTRY *const entry,
    uint8_t **const out, size_t *const out_size)
{
    *out = nullptr;
    *out_size = 0;
    FAIL_IF(
        entry->size > ZIP_MAX_ENTRY_SIZE, "%s is too large to unpack",
        entry->name);
    uint8_t *const buffer = Mem_Alloc(MAX((size_t)entry->size, 1));
    const RESULT result = M_Unpack(data, size, entry, buffer);
    if (!IS_OK(result)) {
        Mem_Free(buffer);
        MUST(result, "%s", entry->name);
    }
    *out = buffer;
    *out_size = entry->size;
    return OK;
}
