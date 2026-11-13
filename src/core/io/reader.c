#include "core/io/reader.h"

#include "core/base/assert.h"
#include "core/base/string.h"
#include "core/base/utils.h"
#include "core/io/backend.h"

#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdckdint.h>
#include <stdio.h>
#include <string.h>

struct READER_FILE {
    SDL_IOStream *stream;
    char *path;
    char *error;
    size_t size;
    size_t buffer_start;
    size_t buffer_length;
    uint8_t buffer[READER_BUFFER_SIZE];
};

static const uint8_t m_NoData[1];

static bool M_ReadStream(
    READER_FILE *const file, const size_t offset, uint8_t *const buffer,
    const size_t size)
{
    bool read = SDL_SeekIO(file->stream, (Sint64)offset, SDL_IO_SEEK_SET) >= 0;
    size_t total = 0;
    while (read && total < size) {
        const size_t count =
            SDL_ReadIO(file->stream, buffer + total, size - total);
        read = count != 0;
        total += count;
    }
    if (!read && file->error == nullptr) {
        file->error = SDL_GetIOStatus(file->stream) == SDL_IO_STATUS_EOF
            ? Str_Dup("the file is shorter than when it was opened")
            : Str_Dup(SDL_GetError());
    }
    return read;
}

// Copies size bytes at offset in the file into buffer, through the block
// that the file keeps in memory. A read of a block or more bypasses it.
static bool M_ReadFile(
    READER_FILE *const file, size_t offset, uint8_t *buffer, size_t size)
{
    while (size > 0) {
        if (offset >= file->buffer_start
            && offset - file->buffer_start < file->buffer_length) {
            const size_t start = offset - file->buffer_start;
            const size_t count = MIN(size, file->buffer_length - start);
            memcpy(buffer, file->buffer + start, count);
            buffer += count;
            offset += count;
            size -= count;
        } else if (size >= READER_BUFFER_SIZE) {
            return M_ReadStream(file, offset, buffer, size);
        } else {
            const size_t length =
                MIN((size_t)READER_BUFFER_SIZE, file->size - offset);
            file->buffer_length = 0;
            if (!M_ReadStream(file, offset, file->buffer, length)) {
                return false;
            }
            file->buffer_start = offset;
            file->buffer_length = length;
        }
    }
    return true;
}

static uint64_t M_Absolute(const READER *const reader, const uint64_t position)
{
    uint64_t offset;
    if (ckd_add(&offset, (uint64_t)reader->origin, position)) {
        return UINT64_MAX;
    }
    return offset;
}

// Returns where the part of path to keep starts, so that " in ...<part>"
// fits in capacity bytes with its terminator: at a '/' when one leaves
// enough room, else at the first whole UTF-8 character that does.
static const char *M_PathTail(const char *const path, const size_t capacity)
{
    const size_t fixed = strlen(" in ...") + 1;
    const size_t length = strlen(path);
    if (capacity <= fixed) {
        return path + length;
    }
    const char *tail = path + length - MIN(length, capacity - fixed);
    for (const char *slash = tail; *slash != '\0'; slash++) {
        if (*slash == '/') {
            return slash;
        }
    }
    while (((unsigned char)*tail & 0xC0) == 0x80) {
        tail++;
    }
    return tail;
}

PRINTF_FORMAT(2, 3)
static void M_Fail(READER *const reader, const char *const fmt, ...)
{
    if (reader->error[0] != '\0') {
        return;
    }
    va_list args;
    va_start(args, fmt);
    vsnprintf(reader->error, sizeof(reader->error), fmt, args);
    va_end(args);
    if (reader->file == nullptr) {
        return;
    }

    const char *const path = reader->file->path;
    const size_t used = strlen(reader->error);
    const size_t capacity = sizeof(reader->error) - used;
    char *const end = reader->error + used;
    if (strlen(" in ") + strlen(path) < capacity) {
        snprintf(end, capacity, " in %s", path);
    } else {
        snprintf(end, capacity, " in ...%s", M_PathTail(path, capacity));
    }
}

static bool M_Claim(
    READER *const reader, const char *const operation, const size_t size,
    size_t *const offset)
{
    if (reader->error[0] != '\0') {
        return false;
    }
    if (size > reader->size - reader->position) {
        M_Fail(
            reader, "%s of %llu bytes at 0x%llx past end 0x%llx", operation,
            (unsigned long long)size,
            (unsigned long long)M_Absolute(reader, reader->position),
            (unsigned long long)M_Absolute(reader, reader->size));
        return false;
    }
    *offset = reader->origin + reader->position;
    reader->position += size;
    return true;
}

static bool M_Copy(READER *const reader, void *const buffer, const size_t size)
{
    size_t offset;
    if (!M_Claim(reader, "read", size, &offset)) {
        return false;
    }
    if (size == 0) {
        return true;
    }
    if (reader->file == nullptr) {
        memcpy(buffer, reader->data + offset, size);
        return true;
    }
    if (!M_ReadFile(reader->file, offset, buffer, size)) {
        reader->position -= size;
        M_Fail(
            reader, "read of %llu bytes at 0x%llx failed (%s)",
            (unsigned long long)size,
            (unsigned long long)M_Absolute(reader, reader->position),
            reader->file->error);
        return false;
    }
    return true;
}

static uint64_t M_ReadLittleEndian(READER *const reader, const size_t size)
{
    uint8_t bytes[sizeof(uint64_t)];
    if (!M_Copy(reader, bytes, size)) {
        return 0;
    }
    uint64_t value = 0;
    for (size_t i = 0; i < size; i++) {
        value |= (uint64_t)bytes[i] << (8 * i);
    }
    return value;
}

void Reader_OpenBuffer(
    READER *const reader, const void *const data, const size_t size)
{
    ASSERT(data != nullptr || size == 0);
    *reader = (READER) {
        .data = data != nullptr ? data : m_NoData,
        .size = size,
    };
}

void Reader_OpenOwnedBuffer(
    READER *const reader, void *const data, const size_t size,
    const ALLOCATOR *const allocator)
{
    Reader_OpenBuffer(reader, data, size);
    reader->buffer_allocator = allocator;
}

RESULT Reader_OpenFile(READER *const reader, const char *const path)
{
    Reader_OpenBuffer(reader, nullptr, 0);
    char *const system_path = IoBackend_SystemPath(path);
    SDL_IOStream *const stream = SDL_IOFromFile(system_path, "rb");
    Mem_Free(system_path);
    FAIL_IF(stream == nullptr, "Cannot open %s: %s", path, SDL_GetError());
    const Sint64 size = SDL_GetIOSize(stream);
    if (size < 0 || (uint64_t)size > SIZE_MAX) {
        const RESULT result = FAIL(
            "Cannot read %s: %s", path,
            size < 0 ? SDL_GetError() : "the file is too large");
        SDL_CloseIO(stream);
        return result;
    }

    READER_FILE *const file = Mem_AllocZ(sizeof(*file));
    file->stream = stream;
    file->path = Str_Dup(path);
    file->size = (size_t)size;
    reader->file = file;
    reader->owns_file = true;
    reader->size = file->size;
    return OK;
}

void Reader_Close(READER *const reader)
{
    if (reader->owns_file) {
        READER_FILE *const file = reader->file;
        SDL_CloseIO(file->stream);
        Mem_Free(file->path);
        Mem_Free(file->error);
        Mem_Free(file);
    } else if (reader->buffer_allocator != nullptr) {
        if (reader->data != m_NoData) {
            Allocator_Free(
                reader->buffer_allocator, (void *)reader->data, reader->size);
        }
    } else {
        return;
    }
    reader->data = m_NoData;
    reader->file = nullptr;
    reader->owns_file = false;
    reader->buffer_allocator = nullptr;
    reader->origin = 0;
    reader->size = 0;
    reader->position = 0;
}

bool Reader_Failed(const READER *const reader)
{
    return reader->error[0] != '\0';
}

RESULT Reader_Result(const READER *const reader)
{
    if (reader->error[0] == '\0') {
        return OK;
    }
    return FAIL("%s", reader->error);
}

size_t Reader_Tell(const READER *const reader)
{
    return reader->position;
}

size_t Reader_Size(const READER *const reader)
{
    return reader->size;
}

size_t Reader_Remaining(const READER *const reader)
{
    return reader->size - reader->position;
}

void Reader_Seek(READER *const reader, const size_t position)
{
    if (reader->error[0] != '\0') {
        return;
    }
    if (position > reader->size) {
        M_Fail(
            reader, "seek to 0x%llx past end 0x%llx",
            (unsigned long long)M_Absolute(reader, position),
            (unsigned long long)M_Absolute(reader, reader->size));
        return;
    }
    reader->position = position;
}

void Reader_Skip(READER *const reader, const size_t size)
{
    size_t offset;
    M_Claim(reader, "skip", size, &offset);
}

uint8_t Reader_ReadU8(READER *const reader)
{
    return (uint8_t)M_ReadLittleEndian(reader, 1);
}

uint16_t Reader_ReadU16(READER *const reader)
{
    return (uint16_t)M_ReadLittleEndian(reader, 2);
}

uint32_t Reader_ReadU32(READER *const reader)
{
    return (uint32_t)M_ReadLittleEndian(reader, 4);
}

uint64_t Reader_ReadU64(READER *const reader)
{
    return M_ReadLittleEndian(reader, 8);
}

int8_t Reader_ReadI8(READER *const reader)
{
    return (int8_t)Reader_ReadU8(reader);
}

int16_t Reader_ReadI16(READER *const reader)
{
    return (int16_t)Reader_ReadU16(reader);
}

int32_t Reader_ReadI32(READER *const reader)
{
    return (int32_t)Reader_ReadU32(reader);
}

int64_t Reader_ReadI64(READER *const reader)
{
    return (int64_t)Reader_ReadU64(reader);
}

float Reader_ReadF32(READER *const reader)
{
    const uint32_t bits = Reader_ReadU32(reader);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

double Reader_ReadF64(READER *const reader)
{
    const uint64_t bits = Reader_ReadU64(reader);
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

void Reader_ReadBytes(
    READER *const reader, void *const buffer, const size_t size)
{
    if (!M_Copy(reader, buffer, size) && size != 0) {
        memset(buffer, 0, size);
    }
}

void *Reader_ReadAlloc(
    READER *const reader, const size_t size, const ALLOCATOR *const allocator)
{
    if (reader->error[0] != '\0') {
        return nullptr;
    }
    if (size > Reader_Remaining(reader)) {
        M_Fail(
            reader, "read of %llu bytes at 0x%llx past end 0x%llx",
            (unsigned long long)size,
            (unsigned long long)M_Absolute(reader, reader->position),
            (unsigned long long)M_Absolute(reader, reader->size));
        return nullptr;
    }
    void *const block = Allocator_Alloc(allocator, size);
    if (!M_Copy(reader, block, size)) {
        Allocator_Free(allocator, block, size);
        return nullptr;
    }
    return block;
}

const void *Reader_Borrow(READER *const reader, const size_t size)
{
    ASSERT(reader->file == nullptr);
    size_t offset;
    if (!M_Claim(reader, "read", size, &offset)) {
        return nullptr;
    }
    return reader->data + offset;
}

READER Reader_Slice(READER *const reader, const size_t size)
{
    READER slice = *reader;
    size_t offset;
    if (!M_Claim(reader, "slice", size, &offset)) {
        slice = *reader;
        slice.owns_file = false;
        slice.buffer_allocator = nullptr;
        slice.size = 0;
        slice.position = 0;
        return slice;
    }
    slice.owns_file = false;
    slice.buffer_allocator = nullptr;
    slice.origin = offset;
    slice.size = size;
    slice.position = 0;
    return slice;
}

void ReaderImpl_ReadU8(READER *const reader, uint8_t *const value)
{
    *value = Reader_ReadU8(reader);
}

void ReaderImpl_ReadU16(READER *const reader, uint16_t *const value)
{
    *value = Reader_ReadU16(reader);
}

void ReaderImpl_ReadU32(READER *const reader, uint32_t *const value)
{
    *value = Reader_ReadU32(reader);
}

void ReaderImpl_ReadU64(READER *const reader, uint64_t *const value)
{
    *value = Reader_ReadU64(reader);
}

void ReaderImpl_ReadI8(READER *const reader, int8_t *const value)
{
    *value = Reader_ReadI8(reader);
}

void ReaderImpl_ReadI16(READER *const reader, int16_t *const value)
{
    *value = Reader_ReadI16(reader);
}

void ReaderImpl_ReadI32(READER *const reader, int32_t *const value)
{
    *value = Reader_ReadI32(reader);
}

void ReaderImpl_ReadI64(READER *const reader, int64_t *const value)
{
    *value = Reader_ReadI64(reader);
}

void ReaderImpl_ReadF32(READER *const reader, float *const value)
{
    *value = Reader_ReadF32(reader);
}

void ReaderImpl_ReadF64(READER *const reader, double *const value)
{
    *value = Reader_ReadF64(reader);
}
