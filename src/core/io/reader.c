#include "core/io/reader.h"

#include "core/base/assert.h"
#include "core/base/utils.h"

#include <stdarg.h>
#include <stdckdint.h>
#include <stdio.h>
#include <string.h>

static const uint8_t m_NoData[1];

static uint64_t M_Absolute(const READER *const reader, const uint64_t position)
{
    uint64_t offset;
    if (ckd_add(&offset, (uint64_t)reader->origin, position)) {
        return UINT64_MAX;
    }
    return offset;
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
    if (size != 0) {
        memcpy(buffer, reader->data + offset, size);
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
        slice.size = 0;
        slice.position = 0;
        return slice;
    }
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
