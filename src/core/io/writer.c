#include "core/io/writer.h"

#include "core/base/assert.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/io/backend.h"

#include <SDL3/SDL.h>
#include <stdckdint.h>
#include <string.h>

#define M_MIN_CAPACITY 64
#define M_FILE_BUFFER_SIZE 65536

// Holds the open temporary file of a file writer, and the block of it that
// is kept in memory: the bytes from buffer_start, which are written to the
// file before another block is started. The system paths reach the file
// system.
struct WRITER_FILE {
    SDL_IOStream *stream;
    char *system_path;
    char *system_temp_path;
    char *error;
    size_t buffer_start;
    size_t buffer_length;
    uint8_t buffer[M_FILE_BUFFER_SIZE];
};

static bool M_IsOpen(const WRITER *const writer)
{
    return writer->allocator != nullptr || writer->file != nullptr;
}

static void M_Fail(WRITER *const writer, const char *const action)
{
    WRITER_FILE *const file = writer->file;
    writer->failed = true;
    file->error =
        Str_Format("Cannot %s %s: %s", action, writer->name, SDL_GetError());
}

static bool M_WriteStream(
    WRITER_FILE *const file, const size_t offset, const uint8_t *const data,
    const size_t size)
{
    if (SDL_SeekIO(file->stream, (Sint64)offset, SDL_IO_SEEK_SET) < 0) {
        return false;
    }
    size_t total = 0;
    while (total < size) {
        const size_t count =
            SDL_WriteIO(file->stream, data + total, size - total);
        if (count == 0) {
            return false;
        }
        total += count;
    }
    return true;
}

static void M_Flush(WRITER *const writer)
{
    WRITER_FILE *const file = writer->file;
    if (writer->failed || file->buffer_length == 0) {
        return;
    }
    if (!M_WriteStream(
            file, file->buffer_start, file->buffer, file->buffer_length)) {
        M_Fail(writer, "write");
        return;
    }
    file->buffer_length = 0;
}

// Writes into the block in memory. A position outside the block, or one
// that would leave a gap in it, starts a new block there.
static void M_WriteFile(WRITER *const writer, const uint8_t *data, size_t size)
{
    WRITER_FILE *const file = writer->file;
    while (size > 0 && !writer->failed) {
        const size_t start = file->buffer_start;
        if (writer->position < start
            || writer->position > start + file->buffer_length
            || writer->position - start >= M_FILE_BUFFER_SIZE) {
            M_Flush(writer);
            file->buffer_start = writer->position;
            file->buffer_length = 0;
            continue;
        }
        const size_t offset = writer->position - start;
        const size_t count = MIN(size, M_FILE_BUFFER_SIZE - offset);
        memcpy(file->buffer + offset, data, count);
        file->buffer_length = MAX(file->buffer_length, offset + count);
        writer->position += count;
        writer->size = MAX(writer->size, writer->position);
        data += count;
        size -= count;
    }
}

static RESULT M_CloseFile(WRITER *const writer)
{
    WRITER_FILE *const file = writer->file;
    M_Flush(writer);
    if (!writer->failed && !SDL_FlushIO(file->stream)) {
        M_Fail(writer, "write");
    }
    if (!SDL_CloseIO(file->stream) && !writer->failed) {
        M_Fail(writer, "write");
    }
    if (!writer->failed
        && !SDL_RenamePath(file->system_temp_path, file->system_path)) {
        M_Fail(writer, "replace");
    }
    RESULT result = OK;
    if (writer->failed) {
        result = FAIL("%s", file->error);
        SDL_RemovePath(file->system_temp_path);
    }
    Mem_Free(file->system_path);
    Mem_Free(file->system_temp_path);
    Mem_Free(file->error);
    Mem_Free(file);
    return result;
}

static void M_Reserve(WRITER *const writer, const size_t end)
{
    if (end <= writer->capacity) {
        return;
    }
    size_t capacity = MAX(writer->capacity, (size_t)M_MIN_CAPACITY);
    while (capacity < end) {
        capacity = capacity <= SIZE_MAX / 2 ? capacity * 2 : end;
    }
    writer->data = Allocator_Realloc(
        writer->allocator, writer->data, writer->capacity, capacity);
    writer->capacity = capacity;
}

static void M_Write(
    WRITER *const writer, const void *const data, const size_t size)
{
    ASSERT(M_IsOpen(writer));
    size_t end;
    const bool overflows = ckd_add(&end, writer->position, size);
    ASSERT(!overflows);
    if (size == 0 || writer->failed) {
        return;
    }
    if (writer->file != nullptr) {
        M_WriteFile(writer, data, size);
        return;
    }
    M_Reserve(writer, end);
    memcpy(writer->data + writer->position, data, size);
    writer->position = end;
    writer->size = MAX(writer->size, end);
}

static void M_WriteLittleEndian(
    WRITER *const writer, const uint64_t value, const size_t size)
{
    uint8_t bytes[sizeof(value)];
    for (size_t i = 0; i < size; i++) {
        bytes[i] = (uint8_t)(value >> (8 * i));
    }
    M_Write(writer, bytes, size);
}

void Writer_OpenBuffer(
    WRITER *const writer, const ALLOCATOR *const allocator,
    const char *const name)
{
    *writer = (WRITER) {
        .allocator = allocator,
        .name = name != nullptr ? Str_Dup(name) : nullptr,
    };
}

RESULT Writer_OpenFile(WRITER *const writer, const char *const path)
{
    *writer = (WRITER) {};
    char *const system_path = IoBackend_SystemPath(path);
    char *const system_temp_path = Str_Format("%s.tmp", system_path);
    SDL_IOStream *const stream = SDL_IOFromFile(system_temp_path, "wb");
    if (stream == nullptr) {
        const RESULT result =
            FAIL("Cannot create %s.tmp: %s", path, SDL_GetError());
        Mem_Free(system_temp_path);
        Mem_Free(system_path);
        return result;
    }
    WRITER_FILE *const file = Mem_AllocZ(sizeof(*file));
    file->stream = stream;
    file->system_path = system_path;
    file->system_temp_path = system_temp_path;
    writer->file = file;
    writer->name = Str_Dup(path);
    return OK;
}

uint8_t *Writer_TakeBuffer(WRITER *const writer, size_t *const size)
{
    ASSERT(writer->file == nullptr);
    ASSERT(M_IsOpen(writer));
    const ALLOCATOR *const allocator = writer->allocator;
    uint8_t *data = writer->data;
    if (data == nullptr) {
        data = Allocator_Alloc(allocator, 0);
    } else if (writer->capacity != writer->size) {
        data =
            Allocator_Realloc(allocator, data, writer->capacity, writer->size);
    }
    *size = writer->size;
    *writer = (WRITER) { .allocator = allocator, .name = writer->name };
    return data;
}

RESULT Writer_Close(WRITER *const writer)
{
    RESULT result = OK;
    if (writer->file != nullptr) {
        result = M_CloseFile(writer);
    } else if (writer->allocator != nullptr) {
        Allocator_Free(writer->allocator, writer->data, writer->capacity);
    }
    Mem_Free(writer->name);
    *writer = (WRITER) {};
    return result;
}

const char *Writer_GetName(const WRITER *const writer)
{
    return writer->name;
}

bool Writer_Failed(const WRITER *const writer)
{
    return writer->failed;
}

RESULT Writer_Result(const WRITER *const writer)
{
    if (!writer->failed) {
        return OK;
    }
    return FAIL("%s", writer->file->error);
}

size_t Writer_Tell(const WRITER *const writer)
{
    return writer->position;
}

void Writer_Seek(WRITER *const writer, const size_t position)
{
    ASSERT(position <= writer->size);
    writer->position = position;
}

void Writer_WriteU8(WRITER *const writer, const uint8_t value)
{
    M_WriteLittleEndian(writer, value, 1);
}

void Writer_WriteU16(WRITER *const writer, const uint16_t value)
{
    M_WriteLittleEndian(writer, value, 2);
}

void Writer_WriteU32(WRITER *const writer, const uint32_t value)
{
    M_WriteLittleEndian(writer, value, 4);
}

void Writer_WriteU64(WRITER *const writer, const uint64_t value)
{
    M_WriteLittleEndian(writer, value, 8);
}

void Writer_WriteI8(WRITER *const writer, const int8_t value)
{
    Writer_WriteU8(writer, (uint8_t)value);
}

void Writer_WriteI16(WRITER *const writer, const int16_t value)
{
    Writer_WriteU16(writer, (uint16_t)value);
}

void Writer_WriteI32(WRITER *const writer, const int32_t value)
{
    Writer_WriteU32(writer, (uint32_t)value);
}

void Writer_WriteI64(WRITER *const writer, const int64_t value)
{
    Writer_WriteU64(writer, (uint64_t)value);
}

void Writer_WriteF32(WRITER *const writer, const float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    Writer_WriteU32(writer, bits);
}

void Writer_WriteF64(WRITER *const writer, const double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    Writer_WriteU64(writer, bits);
}

void Writer_WriteBytes(
    WRITER *const writer, const void *const data, const size_t size)
{
    M_Write(writer, data, size);
}

void WriterImpl_WriteU8(WRITER *const writer, const uint8_t *const value)
{
    Writer_WriteU8(writer, *value);
}

void WriterImpl_WriteU16(WRITER *const writer, const uint16_t *const value)
{
    Writer_WriteU16(writer, *value);
}

void WriterImpl_WriteU32(WRITER *const writer, const uint32_t *const value)
{
    Writer_WriteU32(writer, *value);
}

void WriterImpl_WriteU64(WRITER *const writer, const uint64_t *const value)
{
    Writer_WriteU64(writer, *value);
}

void WriterImpl_WriteI8(WRITER *const writer, const int8_t *const value)
{
    Writer_WriteI8(writer, *value);
}

void WriterImpl_WriteI16(WRITER *const writer, const int16_t *const value)
{
    Writer_WriteI16(writer, *value);
}

void WriterImpl_WriteI32(WRITER *const writer, const int32_t *const value)
{
    Writer_WriteI32(writer, *value);
}

void WriterImpl_WriteI64(WRITER *const writer, const int64_t *const value)
{
    Writer_WriteI64(writer, *value);
}

void WriterImpl_WriteF32(WRITER *const writer, const float *const value)
{
    Writer_WriteF32(writer, *value);
}

void WriterImpl_WriteF64(WRITER *const writer, const double *const value)
{
    Writer_WriteF64(writer, *value);
}
