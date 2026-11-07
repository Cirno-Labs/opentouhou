#include "core/io/writer.h"

#include "core/base/assert.h"
#include "core/base/utils.h"

#include <stdckdint.h>
#include <string.h>

#define M_MIN_CAPACITY 64

static bool M_IsOpen(const WRITER *const writer)
{
    return writer->allocator != nullptr;
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
    if (size == 0) {
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

void Writer_OpenBuffer(WRITER *const writer, const ALLOCATOR *const allocator)
{
    *writer = (WRITER) { .allocator = allocator };
}

uint8_t *Writer_TakeBuffer(WRITER *const writer, size_t *const size)
{
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
    Writer_OpenBuffer(writer, allocator);
    return data;
}

RESULT Writer_Close(WRITER *const writer)
{
    if (M_IsOpen(writer)) {
        Allocator_Free(writer->allocator, writer->data, writer->capacity);
    }
    *writer = (WRITER) {};
    return OK;
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
