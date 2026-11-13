#pragma once

// Builds zip archives in memory for the othpatch tests.

#include "core/base/memory.h"
#include "core/hash/crc32.h"

#include "test.h"

#include <string.h>
#include <zlib.h>

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

static void M_ZipPut16(uint8_t **const out, const uint32_t value)
{
    (*out)[0] = (uint8_t)value;
    (*out)[1] = (uint8_t)(value >> 8);
    *out += 2;
}

static void M_ZipPut32(uint8_t **const out, const uint32_t value)
{
    M_ZipPut16(out, value & 0xFFFF);
    M_ZipPut16(out, value >> 16);
}

static void M_ZipPutBytes(
    uint8_t **const out, const void *const data, const size_t size)
{
    if (size > 0) {
        memcpy(*out, data, size);
    }
    *out += size;
}

// Returns the raw deflate stream of data. Mem_Free releases it.
static uint8_t *M_ZipDeflate(
    const void *const data, const size_t size, size_t *const out_size)
{
    const size_t capacity = compressBound((uLong)size) + 64;
    uint8_t *const out = Mem_Alloc(capacity);
    z_stream stream = {};
    CHECK(
        deflateInit2(
            &stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8,
            Z_DEFAULT_STRATEGY)
        == Z_OK);
    stream.next_in = (Bytef *)data;
    stream.avail_in = (uInt)size;
    stream.next_out = out;
    stream.avail_out = (uInt)capacity;
    CHECK(deflate(&stream, Z_FINISH) == Z_STREAM_END);
    *out_size = stream.total_out;
    deflateEnd(&stream);
    return out;
}

// Returns an archive of count entries.
[[maybe_unused]] static ZIP_BUILDER_ARCHIVE ZipBuilder_Build(
    const ZIP_BUILDER_ENTRY *const entries, const int count)
{
    uint8_t **const packed = Mem_AllocArrZ((size_t)count, sizeof(*packed));
    size_t *const packed_sizes =
        Mem_AllocArrZ((size_t)count, sizeof(*packed_sizes));
    size_t capacity = 22;
    for (int i = 0; i < count; i++) {
        const ZIP_BUILDER_ENTRY *const entry = &entries[i];
        if (entry->deflate) {
            packed[i] =
                M_ZipDeflate(entry->data, entry->size, &packed_sizes[i]);
        } else {
            packed_sizes[i] = entry->size;
        }
        capacity += 30 + 46 + 2 * strlen(entry->name) + packed_sizes[i];
    }
    uint8_t *const data = Mem_Alloc(capacity);
    uint8_t *cursor = data;
    uint32_t *const offsets = Mem_AllocArrZ((size_t)count, sizeof(*offsets));
    for (int i = 0; i < count; i++) {
        const ZIP_BUILDER_ENTRY *const entry = &entries[i];
        offsets[i] = (uint32_t)(cursor - data);
        M_ZipPut32(&cursor, 0x04034B50);
        M_ZipPut16(&cursor, 20);
        M_ZipPut16(&cursor, 0);
        M_ZipPut16(&cursor, entry->deflate ? 8 : 0);
        M_ZipPut32(&cursor, 0);
        M_ZipPut32(&cursor, Crc32_Compute(entry->data, entry->size));
        M_ZipPut32(&cursor, (uint32_t)packed_sizes[i]);
        M_ZipPut32(&cursor, (uint32_t)entry->size);
        M_ZipPut16(&cursor, (uint32_t)strlen(entry->name));
        M_ZipPut16(&cursor, 0);
        M_ZipPutBytes(&cursor, entry->name, strlen(entry->name));
        M_ZipPutBytes(
            &cursor, entry->deflate ? packed[i] : entry->data, packed_sizes[i]);
    }
    const uint32_t directory_offset = (uint32_t)(cursor - data);
    for (int i = 0; i < count; i++) {
        const ZIP_BUILDER_ENTRY *const entry = &entries[i];
        M_ZipPut32(&cursor, 0x02014B50);
        M_ZipPut16(&cursor, entry->mode != 0 ? 3 << 8 | 20 : 20);
        M_ZipPut16(&cursor, 20);
        M_ZipPut16(&cursor, 0);
        M_ZipPut16(&cursor, entry->deflate ? 8 : 0);
        M_ZipPut32(&cursor, 0);
        M_ZipPut32(&cursor, Crc32_Compute(entry->data, entry->size));
        M_ZipPut32(&cursor, (uint32_t)packed_sizes[i]);
        M_ZipPut32(&cursor, (uint32_t)entry->size);
        M_ZipPut16(&cursor, (uint32_t)strlen(entry->name));
        M_ZipPut16(&cursor, 0);
        M_ZipPut16(&cursor, 0);
        M_ZipPut16(&cursor, 0);
        M_ZipPut16(&cursor, 0);
        M_ZipPut32(&cursor, entry->mode << 16);
        M_ZipPut32(&cursor, offsets[i]);
        M_ZipPutBytes(&cursor, entry->name, strlen(entry->name));
    }
    const uint32_t directory_size =
        (uint32_t)(cursor - data) - directory_offset;
    M_ZipPut32(&cursor, 0x06054B50);
    M_ZipPut16(&cursor, 0);
    M_ZipPut16(&cursor, 0);
    M_ZipPut16(&cursor, (uint32_t)count);
    M_ZipPut16(&cursor, (uint32_t)count);
    M_ZipPut32(&cursor, directory_size);
    M_ZipPut32(&cursor, directory_offset);
    M_ZipPut16(&cursor, 0);
    for (int i = 0; i < count; i++) {
        Mem_Free(packed[i]);
    }
    Mem_Free(offsets);
    Mem_Free(packed_sizes);
    Mem_Free(packed);
    return (ZIP_BUILDER_ARCHIVE) {
        .data = data,
        .size = (size_t)(cursor - data),
    };
}
