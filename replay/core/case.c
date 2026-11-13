// SPDX-License-Identifier: GPL-3.0-only

#include "case.h"

#include "core/base/assert.h"
#include "core/base/memory.h"
#include "core/base/utils.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

// Sets how many bytes the zlib streams move at a time, in either
// direction, so a case of any size is read and written without holding it
// whole in memory.
#define M_ZLIB_BUFFER_SIZE 65536

// Sets the size of the buffer that holds a CASE_READER's failure message.
#define M_READER_ERROR_SIZE 160

struct CASE_WRITER {
    WRITER *out;
    int32_t count;
    uint32_t *previous;
    uint8_t *row_bytes;
    z_stream stream;
};

struct CASE_READER {
    READER *in;
    CASE_HEADER header;
    z_stream stream;
    uint32_t *row;
    uint32_t *previous;
    uint8_t *row_bytes;
    uint8_t in_buffer[M_ZLIB_BUFFER_SIZE];
    bool ended;
    bool failed;
    char error[M_READER_ERROR_SIZE];
};

static void M_CheckNames(const CASE_NAMES *const names)
{
    for (int32_t i = 0; i < names->count; i++) {
        const size_t length = strlen(names->items[i]);
        ASSERT(length > 0 && length <= 255);
        for (int32_t j = 0; j < i; j++) {
            ASSERT(strcmp(names->items[i], names->items[j]) != 0);
        }
    }
}

static void M_WriteNames(WRITER *const out, const CASE_NAMES *const names)
{
    const uint32_t count = (uint32_t)names->count;
    Writer_Write(out, &count);
    for (int32_t i = 0; i < names->count; i++) {
        const uint8_t length = (uint8_t)strlen(names->items[i]);
        Writer_Write(out, &length);
        Writer_WriteBytes(out, names->items[i], length);
    }
}

// Runs data through the writer's deflate stream and sends the compressed
// bytes it produces to out. A flush of Z_FINISH drains the stream and
// ends it with Z_STREAM_END.
static void M_Deflate(
    CASE_WRITER *const writer, const uint8_t *const data, const size_t size,
    const int flush)
{
    uint8_t out_buffer[M_ZLIB_BUFFER_SIZE];
    writer->stream.next_in = (uint8_t *)data;
    writer->stream.avail_in = (uInt)size;
    for (;;) {
        writer->stream.next_out = out_buffer;
        writer->stream.avail_out = sizeof(out_buffer);
        const int ret = deflate(&writer->stream, flush);
        ASSERT(ret != Z_STREAM_ERROR);
        const size_t produced = sizeof(out_buffer) - writer->stream.avail_out;
        if (produced > 0) {
            Writer_WriteBytes(writer->out, out_buffer, produced);
        }
        if (flush == Z_FINISH) {
            if (ret == Z_STREAM_END) {
                break;
            }
        } else if (writer->stream.avail_out != 0) {
            break;
        }
    }
}

CASE_WRITER *Case_OpenWriter(WRITER *const out, const CASE_HEADER *const header)
{
    ASSERT(header->start.count == header->start_names.count);
    ASSERT(strlen(header->game_id) <= CASE_GAME_ID_SIZE);
    M_CheckNames(&header->start_names);
    M_CheckNames(&header->calc_names);
    ASSERT(header->calc_names.count > 0);

    Writer_WriteBytes(out, CASE_MAGIC, 4);
    const uint16_t version = CASE_VERSION;
    Writer_Write(out, &version);
    uint8_t game_id[CASE_GAME_ID_SIZE] = {};
    memcpy(game_id, header->game_id, strlen(header->game_id));
    Writer_WriteBytes(out, game_id, CASE_GAME_ID_SIZE);
    const uint32_t info_length = (uint32_t)strlen(header->info);
    Writer_Write(out, &info_length);
    Writer_WriteBytes(out, header->info, info_length);
    M_WriteNames(out, &header->start_names);
    for (int32_t i = 0; i < header->start.count; i++) {
        Writer_Write(out, &header->start.items[i]);
    }
    M_WriteNames(out, &header->calc_names);

    CASE_WRITER *const writer = Mem_AllocZ(sizeof(*writer));
    writer->out = out;
    writer->count = header->calc_names.count;
    writer->previous = Mem_AllocArrZ(writer->count, sizeof(uint32_t));
    writer->row_bytes = Mem_AllocArr(writer->count, 4);
    const int ret = deflateInit(&writer->stream, Z_DEFAULT_COMPRESSION);
    ASSERT(ret == Z_OK);
    return writer;
}

void Case_WriteRow(CASE_WRITER *const writer, const uint32_t *const row)
{
    for (int32_t i = 0; i < writer->count; i++) {
        const uint32_t value = row[i] ^ writer->previous[i];
        writer->row_bytes[i * 4 + 0] = (uint8_t)(value);
        writer->row_bytes[i * 4 + 1] = (uint8_t)(value >> 8);
        writer->row_bytes[i * 4 + 2] = (uint8_t)(value >> 16);
        writer->row_bytes[i * 4 + 3] = (uint8_t)(value >> 24);
        writer->previous[i] = row[i];
    }
    M_Deflate(writer, writer->row_bytes, (size_t)writer->count * 4, Z_NO_FLUSH);
}

RESULT Case_CloseWriter(CASE_WRITER *const writer)
{
    M_Deflate(writer, nullptr, 0, Z_FINISH);
    deflateEnd(&writer->stream);
    const RESULT result = Writer_Result(writer->out);
    Mem_Free(writer->previous);
    Mem_Free(writer->row_bytes);
    Mem_Free(writer);
    return result;
}

static RESULT M_ReadNames(
    READER *const in, ARENA *const arena, CASE_NAMES *const names)
{
    uint32_t count;
    Reader_Read(in, &count);
    FAIL_IF(Reader_Failed(in), "truncated case header");
    FAIL_IF(count > Reader_Remaining(in), "case header: name count too large");

    Array_Init(names, Arena_Allocator(arena));
    for (uint32_t i = 0; i < count; i++) {
        uint8_t length;
        Reader_Read(in, &length);
        FAIL_IF(Reader_Failed(in), "truncated case header");
        FAIL_IF(length == 0, "empty column name");
        FAIL_IF(length > Reader_Remaining(in), "truncated case header");

        char *const name = Arena_Alloc(arena, (size_t)length + 1);
        Reader_ReadBytes(in, name, length);
        FAIL_IF(Reader_Failed(in), "truncated case header");
        name[length] = '\0';
        FAIL_IF(
            Case_FindColumn(names, name) >= 0, "duplicate column name \"%s\"",
            name);
        Array_Append(names, (const char *)name);
    }
    return OK;
}

RESULT Case_OpenReader(
    READER *const in, ARENA *const arena, CASE_READER **const out)
{
    uint8_t magic[4];
    Reader_ReadBytes(in, magic, sizeof(magic));
    FAIL_IF(Reader_Failed(in), "truncated case header");
    FAIL_IF(memcmp(magic, CASE_MAGIC, 4) != 0, "not a case file");

    uint16_t version;
    Reader_Read(in, &version);
    FAIL_IF(Reader_Failed(in), "truncated case header");
    FAIL_IF(
        version != CASE_VERSION, "case version %d, expected %d", version,
        CASE_VERSION);

    char game_id[CASE_GAME_ID_SIZE];
    Reader_ReadBytes(in, game_id, sizeof(game_id));
    FAIL_IF(Reader_Failed(in), "truncated case header");

    uint32_t info_length;
    Reader_Read(in, &info_length);
    FAIL_IF(Reader_Failed(in), "truncated case header");
    FAIL_IF(info_length > Reader_Remaining(in), "truncated case header");

    CASE_READER *const reader = Arena_AllocZ(arena, sizeof(*reader));
    reader->in = in;
    memcpy(reader->header.game_id, game_id, CASE_GAME_ID_SIZE);
    reader->header.game_id[CASE_GAME_ID_SIZE] = '\0';

    char *const info = Arena_Alloc(arena, (size_t)info_length + 1);
    Reader_ReadBytes(in, info, info_length);
    FAIL_IF(Reader_Failed(in), "truncated case header");
    info[info_length] = '\0';
    reader->header.info = info;

    MUST(M_ReadNames(in, arena, &reader->header.start_names));

    const uint32_t start_count = (uint32_t)reader->header.start_names.count;
    FAIL_IF(
        (size_t)start_count * 4 > Reader_Remaining(in),
        "truncated case header");
    Array_Init(&reader->header.start, Arena_Allocator(arena));
    for (uint32_t i = 0; i < start_count; i++) {
        uint32_t value;
        Reader_Read(in, &value);
        Array_Append(&reader->header.start, value);
    }
    FAIL_IF(Reader_Failed(in), "truncated case header");

    MUST(M_ReadNames(in, arena, &reader->header.calc_names));

    const int32_t count = reader->header.calc_names.count;
    FAIL_IF(count == 0, "case has no calc columns");
    reader->row = Arena_AllocArr(arena, (size_t)count, sizeof(uint32_t));
    reader->previous = Arena_AllocArrZ(arena, (size_t)count, sizeof(uint32_t));
    reader->row_bytes = Arena_AllocArr(arena, (size_t)count, 4);
    const int ret = inflateInit(&reader->stream);
    ASSERT(ret == Z_OK);

    *out = reader;
    return OK;
}

const CASE_HEADER *Case_GetHeader(const CASE_READER *const reader)
{
    return &reader->header;
}

PRINTF_FORMAT(2, 3)
static void M_Fail(CASE_READER *const reader, const char *const fmt, ...)
{
    if (reader->failed) {
        return;
    }
    reader->failed = true;
    va_list args;
    va_start(args, fmt);
    vsnprintf(reader->error, sizeof(reader->error), fmt, args);
    va_end(args);
}

// Reads the next chunk of compressed bytes into the reader's input buffer
// when its stream has none left. At the end of in, the stream gets no
// input, because inflate can still have output pending. Returns false when
// the read fails.
static bool M_Refill(CASE_READER *const reader)
{
    if (reader->stream.avail_in != 0) {
        return true;
    }
    const size_t remaining = Reader_Remaining(reader->in);
    const size_t want = remaining < sizeof(reader->in_buffer)
        ? remaining
        : sizeof(reader->in_buffer);
    if (want == 0) {
        return true;
    }
    Reader_ReadBytes(reader->in, reader->in_buffer, want);
    if (Reader_Failed(reader->in)) {
        M_Fail(reader, "failed to read case stream");
        return false;
    }
    reader->stream.avail_in = (uInt)want;
    reader->stream.next_in = reader->in_buffer;
    return true;
}

const uint32_t *Case_ReadRow(CASE_READER *const reader)
{
    if (reader->failed || reader->ended) {
        return nullptr;
    }
    const int32_t count = reader->header.calc_names.count;
    const size_t target = (size_t)count * 4;
    size_t produced = 0;
    while (produced < target) {
        if (!M_Refill(reader)) {
            return nullptr;
        }
        reader->stream.next_out = reader->row_bytes + produced;
        reader->stream.avail_out = (uInt)(target - produced);
        const uLong before = reader->stream.total_out;
        const int ret = inflate(&reader->stream, Z_NO_FLUSH);
        produced += (size_t)(reader->stream.total_out - before);

        if (ret == Z_STREAM_END) {
            if (produced == 0) {
                if (reader->stream.avail_in != 0
                    || Reader_Remaining(reader->in) != 0) {
                    M_Fail(reader, "trailing bytes after case stream");
                    return nullptr;
                }
                reader->ended = true;
                return nullptr;
            }
            if (produced != target) {
                M_Fail(reader, "truncated case stream: partial row");
                return nullptr;
            }
            if (reader->stream.avail_in != 0
                || Reader_Remaining(reader->in) != 0) {
                M_Fail(reader, "trailing bytes after case stream");
                return nullptr;
            }
            reader->ended = true;
            break;
        }
        if (ret != Z_OK && ret != Z_BUF_ERROR) {
            M_Fail(reader, "corrupt case stream");
            return nullptr;
        }
        if (ret == Z_BUF_ERROR && reader->stream.total_out == before) {
            M_Fail(
                reader,
                reader->stream.avail_in != 0 ? "corrupt case stream"
                                             : "truncated case stream");
            return nullptr;
        }
    }

    for (int32_t i = 0; i < count; i++) {
        const uint32_t value = (uint32_t)reader->row_bytes[i * 4 + 0]
            | (uint32_t)reader->row_bytes[i * 4 + 1] << 8
            | (uint32_t)reader->row_bytes[i * 4 + 2] << 16
            | (uint32_t)reader->row_bytes[i * 4 + 3] << 24;
        reader->row[i] = value ^ reader->previous[i];
        reader->previous[i] = reader->row[i];
    }
    return reader->row;
}

RESULT Case_CloseReader(CASE_READER *const reader)
{
    inflateEnd(&reader->stream);
    if (reader->failed) {
        return FAIL("%s", reader->error);
    }
    return OK;
}

int32_t Case_FindColumn(const CASE_NAMES *const names, const char *const name)
{
    for (int32_t i = 0; i < names->count; i++) {
        if (strcmp(names->items[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

uint32_t Case_Hash(uint32_t hash, const uint32_t value)
{
    const uint8_t bytes[4] = {
        (uint8_t)(value),
        (uint8_t)(value >> 8),
        (uint8_t)(value >> 16),
        (uint8_t)(value >> 24),
    };
    for (int i = 0; i < 4; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}
