#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/base/types.h"

// Writes little-endian values, as READER reads them, into a growable buffer.
// Every file format that the program writes is written field by field
// through a WRITER, so no structure is ever written as it lies in memory.
//
// Writes go to the position and move it forward. Writer_Seek moves the
// position back, to replace a field that was written before, such as a size
// or an offset that is known only after the data it describes. A write that
// passes the end extends the data.
//
// Running out of memory is fatal. Writing to a closed writer, seeking past
// the end and taking the buffer of a closed writer are programming errors,
// and fatal too.
//
// Example:
// WRITER writer;
// Writer_OpenBuffer(&writer, &g_HeapAllocator);
// const uint32_t version = 2;
// Writer_Write(&writer, &version);
// const size_t size_position = Writer_Tell(&writer);
// Writer_WriteU32(&writer, 0);
// M_WriteEntries(&writer);
// const size_t end = Writer_Tell(&writer);
// Writer_Seek(&writer, size_position);
// Writer_WriteU32(&writer, (uint32_t)(end - size_position - 4));
// Writer_Seek(&writer, end);
// size_t size;
// uint8_t *const data = Writer_TakeBuffer(&writer, &size);

// Holds a destination and a position in it. The fields are private to the
// Writer_ functions. A WRITER is a plain value, so it can live on the stack.
typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
    size_t position;
    const ALLOCATOR *allocator;
} WRITER;

// Opens a writer into a growable buffer that the writer owns, with its
// memory from allocator. It allocates nothing until the first write.
void Writer_OpenBuffer(WRITER *writer, const ALLOCATOR *allocator);

// Hands the bytes of a buffer writer to the caller, and sets *size to their
// number. Allocator_Free(allocator, data, *size), with the allocator of the
// writer, releases them. The writer is left as an empty buffer writer with
// the same allocator.
uint8_t *Writer_TakeBuffer(WRITER *writer, size_t *size);

// Closes a writer. For a buffer writer, releases the bytes that
// Writer_TakeBuffer did not take, and returns OK. Closing a closed writer
// does nothing.
RESULT Writer_Close(WRITER *writer);

// Returns the position, which is the offset of the next write.
size_t Writer_Tell(const WRITER *writer);

// Moves to position, which must not be past the end of the data.
void Writer_Seek(WRITER *writer, size_t position);

// Write one value at the position and move past it.
void Writer_WriteU8(WRITER *writer, uint8_t value);
void Writer_WriteU16(WRITER *writer, uint16_t value);
void Writer_WriteU32(WRITER *writer, uint32_t value);
void Writer_WriteU64(WRITER *writer, uint64_t value);
void Writer_WriteI8(WRITER *writer, int8_t value);
void Writer_WriteI16(WRITER *writer, int16_t value);
void Writer_WriteI32(WRITER *writer, int32_t value);
void Writer_WriteI64(WRITER *writer, int64_t value);

// Write an IEEE 754 value as its bits, unchanged.
void Writer_WriteF32(WRITER *writer, float value);
void Writer_WriteF64(WRITER *writer, double value);

// Writes the variable that field points to, with the function above that
// matches its type. The variable may be const. A pointer to any other type,
// and a value that is not a pointer, fail to compile.
//
// Example:
// const uint16_t flags = 0x0102;
// Writer_Write(&writer, &flags);
#define Writer_Write(writer_, field_)                                          \
    _Generic(                                                                  \
        (typeof_unqual(*(field_)) *)nullptr,                                   \
        uint8_t *: WriterImpl_WriteU8,                                         \
        uint16_t *: WriterImpl_WriteU16,                                       \
        uint32_t *: WriterImpl_WriteU32,                                       \
        uint64_t *: WriterImpl_WriteU64,                                       \
        int8_t *: WriterImpl_WriteI8,                                          \
        int16_t *: WriterImpl_WriteI16,                                        \
        int32_t *: WriterImpl_WriteI32,                                        \
        int64_t *: WriterImpl_WriteI64,                                        \
        float *: WriterImpl_WriteF32,                                          \
        double *: WriterImpl_WriteF64)((writer_), (field_))

// Writes size bytes of data. data must not point into the writer's buffer.
void Writer_WriteBytes(WRITER *writer, const void *data, size_t size);

// The rest of this header is the implementation behind Writer_Write.

void WriterImpl_WriteU8(WRITER *writer, const uint8_t *value);
void WriterImpl_WriteU16(WRITER *writer, const uint16_t *value);
void WriterImpl_WriteU32(WRITER *writer, const uint32_t *value);
void WriterImpl_WriteU64(WRITER *writer, const uint64_t *value);
void WriterImpl_WriteI8(WRITER *writer, const int8_t *value);
void WriterImpl_WriteI16(WRITER *writer, const int16_t *value);
void WriterImpl_WriteI32(WRITER *writer, const int32_t *value);
void WriterImpl_WriteI64(WRITER *writer, const int64_t *value);
void WriterImpl_WriteF32(WRITER *writer, const float *value);
void WriterImpl_WriteF64(WRITER *writer, const double *value);
