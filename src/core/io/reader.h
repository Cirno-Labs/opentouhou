#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/base/types.h"

// Reads little-endian values from a buffer in memory or from a file. Every
// file format is parsed field by field through a READER, so no structure is
// ever filled straight from file data, and the code does not depend on
// packing, pointer size or byte order.
//
// The data is untrusted, so no read is fatal and no read goes outside the
// reader's range. The first read that fails, by passing the end or by an
// error of the file, sets the error flag and records the failure. After that,
// every read returns zero, Reader_ReadBytes fills its buffer with zeros,
// Reader_ReadAlloc and Reader_Borrow return nullptr, and Reader_Skip and
// Reader_Seek do nothing. A parser therefore reads its fields without checking
// each one, and calls Reader_Result once at the end. A value that sizes an
// allocation or a loop must be checked before it is used, with Reader_Failed or
// against Reader_Remaining.
//
// Positions count from the start of the reader's range. The offsets in
// failure messages count from the start of the buffer or the file, also in a
// slice, so that they match the offsets in the file.
//
// Example:
// READER reader;
// MUST(Reader_OpenFile(&reader, path));
// uint32_t version;
// uint32_t count;
// Reader_Read(&reader, &version);
// Reader_Read(&reader, &count);
// Reader_Close(&reader);
// MUST(Reader_Result(&reader));

// Sets how many bytes a file reader reads from the file at a time.
#define READER_BUFFER_SIZE 65536

// Sets the size of the buffer that holds a reader's failure message.
#define READER_ERROR_SIZE 160

typedef struct READER_FILE READER_FILE;

// Holds a range of a source and a position in it. The fields are private
// to the Reader_ functions. A READER is a plain value, so it can live on
// the stack.
typedef struct {
    const uint8_t *data;
    READER_FILE *file;
    bool owns_file;
    const ALLOCATOR *buffer_allocator;
    size_t origin;
    size_t size;
    size_t position;
    // Holds the message of the first operation that failed, formatted when
    // it failed, or an empty string while none has failed.
    char error[READER_ERROR_SIZE];
} READER;

// Prepares a reader over size bytes of data, at position 0. The reader never
// owns the data, which must stay valid and unchanged while the reader and
// its slices are in use. A data of nullptr is allowed when size is 0.
void Reader_OpenBuffer(READER *reader, const void *data, size_t size);

// Prepares a reader over size bytes of data, at position 0, and gives the
// reader ownership of data: Reader_Close releases it with allocator.
// Reader_Borrow works on it like on any buffer reader, but the pointers it
// returns, and the ranges of any slice taken before the close, stay valid
// only until Reader_Close runs.
void Reader_OpenOwnedBuffer(
    READER *reader, void *data, size_t size, const ALLOCATOR *allocator);

// Opens a file for reading, at position 0. The reader reads the file in
// blocks of READER_BUFFER_SIZE bytes as the position reaches them, and never
// holds the whole file in memory. On failure, the reader is empty, and
// Reader_Close does nothing.
RESULT Reader_OpenFile(READER *reader, const char *path);

// Closes the file of a reader that Reader_OpenFile opened, or releases the
// data of a reader that Reader_OpenOwnedBuffer opened, and leaves the reader
// empty. The slices of the reader become invalid. The error flag and its
// message stay unchanged. For a reader that owns neither, and for a slice,
// does nothing.
void Reader_Close(READER *reader);

// Returns whether the error flag is set.
bool Reader_Failed(const READER *reader);

// Returns a failure that names the first failed operation and its offset,
// such as "read of 4 bytes at 0x1234 past end 0x1230", or OK when the error
// flag is not set. For a file, an error of the file adds the reason that
// the platform gives, and the message ends with " in <path>". When the path
// does not fit in READER_ERROR_SIZE, its start is replaced with "...", cut
// at a '/' where possible, so the file name stays whole. The message was
// formatted when the operation failed, so it still names the path after
// Reader_Close.
RESULT Reader_Result(const READER *reader);

// Return the position, the size of the range, and the number of bytes from
// the position to the end of the range.
size_t Reader_Tell(const READER *reader);
size_t Reader_Size(const READER *reader);
size_t Reader_Remaining(const READER *reader);

// Moves to position. The end of the range is a valid position; a position
// past it is a failure.
void Reader_Seek(READER *reader, size_t position);

// Moves forward by size bytes.
void Reader_Skip(READER *reader, size_t size);

// Read one value at the position and move past it.
uint8_t Reader_ReadU8(READER *reader);
uint16_t Reader_ReadU16(READER *reader);
uint32_t Reader_ReadU32(READER *reader);
uint64_t Reader_ReadU64(READER *reader);
int8_t Reader_ReadI8(READER *reader);
int16_t Reader_ReadI16(READER *reader);
int32_t Reader_ReadI32(READER *reader);
int64_t Reader_ReadI64(READER *reader);

// Read an IEEE 754 value stored as its bits. The value has exactly the bits
// that were read, also for NaN payloads, negative zero and subnormals.
float Reader_ReadF32(READER *reader);
double Reader_ReadF64(READER *reader);

// Reads a value into the variable that field points to, with the function
// above that matches its type. A pointer to any other type fails to
// compile.
//
// Example:
// uint16_t flags;
// float speed;
// Reader_Read(&reader, &flags);
// Reader_Read(&reader, &speed);
#define Reader_Read(reader_, field_)                                           \
    _Generic(                                                                  \
        (field_),                                                              \
        uint8_t *: ReaderImpl_ReadU8,                                          \
        uint16_t *: ReaderImpl_ReadU16,                                        \
        uint32_t *: ReaderImpl_ReadU32,                                        \
        uint64_t *: ReaderImpl_ReadU64,                                        \
        int8_t *: ReaderImpl_ReadI8,                                           \
        int16_t *: ReaderImpl_ReadI16,                                         \
        int32_t *: ReaderImpl_ReadI32,                                         \
        int64_t *: ReaderImpl_ReadI64,                                         \
        float *: ReaderImpl_ReadF32,                                           \
        double *: ReaderImpl_ReadF64)((reader_), (field_))

// Copies size bytes into buffer and moves past them. On failure, buffer is
// filled with zeros.
void Reader_ReadBytes(READER *reader, void *buffer, size_t size);

// Copies size bytes into a new block from allocator and moves past them.
// Allocator_Free(allocator, block, size) releases the block. The size is
// checked against the range before anything is allocated, so a size read
// from the data cannot cause a large allocation. On failure, returns
// nullptr and allocates nothing.
void *Reader_ReadAlloc(READER *reader, size_t size, const ALLOCATOR *allocator);

// Returns a pointer to the next size bytes of the buffer, without copying
// them, and moves past them. The pointer points into the memory given to
// Reader_OpenBuffer, so it stays valid for as long as that memory does,
// independently of the reader. On failure, returns nullptr. A file reader
// has no memory to point into, so calling this on one is fatal.
const void *Reader_Borrow(READER *reader, size_t size);

// Returns a reader over the next size bytes, and moves this reader past
// them. The slice has its own position and error flag. A slice of a file
// reader reads through the file of the reader that Reader_OpenFile opened,
// so it is valid until that reader is closed, and needs no Reader_Close of
// its own. On failure, the slice is empty and has the failure of this
// reader, so that the result of either one reports it.
READER Reader_Slice(READER *reader, size_t size);

// The rest of this header is the implementation behind Reader_Read.

void ReaderImpl_ReadU8(READER *reader, uint8_t *value);
void ReaderImpl_ReadU16(READER *reader, uint16_t *value);
void ReaderImpl_ReadU32(READER *reader, uint32_t *value);
void ReaderImpl_ReadU64(READER *reader, uint64_t *value);
void ReaderImpl_ReadI8(READER *reader, int8_t *value);
void ReaderImpl_ReadI16(READER *reader, int16_t *value);
void ReaderImpl_ReadI32(READER *reader, int32_t *value);
void ReaderImpl_ReadI64(READER *reader, int64_t *value);
void ReaderImpl_ReadF32(READER *reader, float *value);
void ReaderImpl_ReadF64(READER *reader, double *value);
