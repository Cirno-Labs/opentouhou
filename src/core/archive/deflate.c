#include "core/archive/deflate.h"

#include "core/base/utils.h"

#include <string.h>
#include <zlib.h>

#define M_SPAN ((size_t)4 * 1024 * 1024)
#define M_BUFFER_SIZE 65536

typedef struct {
    z_stream stream;
    size_t input_offset;
} M_CHECKPOINT;

struct DEFLATE_INDEX {
    const ALLOCATOR *allocator;
    M_CHECKPOINT *points;
    size_t capacity;
    size_t count;
};

typedef struct {
    READER packed;
    const ALLOCATOR *allocator;
    DEFLATE_INDEX *index;
    z_stream stream;
    bool initialized;
    bool ended;
    size_t position;
    uint8_t input[M_BUFFER_SIZE];
} M_DEFLATE;

static RESULT M_Decode(
    M_DEFLATE *const source, uint8_t *const out, const size_t size,
    size_t *const produced)
{
    *produced = 0;
    while (*produced < size && !source->ended) {
        if (source->stream.avail_in == 0) {
            const size_t count =
                MIN(Reader_Remaining(&source->packed), M_BUFFER_SIZE);
            Reader_ReadBytes(&source->packed, source->input, count);
            MUST(Reader_Result(&source->packed));
            source->stream.next_in = source->input;
            source->stream.avail_in = (uInt)count;
        }
        source->stream.next_out = out + *produced;
        source->stream.avail_out = (uInt)(size - *produced);
        const uInt before_in = source->stream.avail_in;
        const uInt before_out = source->stream.avail_out;
        const int status = inflate(&source->stream, Z_NO_FLUSH);
        const size_t count = before_out - source->stream.avail_out;
        *produced += count;
        source->position += count;
        source->ended = status == Z_STREAM_END;
        FAIL_IF(
            status != Z_OK && status != Z_STREAM_END,
            "the packed data is damaged");
        FAIL_IF(
            !source->ended && count == 0
                && before_in == source->stream.avail_in,
            "the packed data is truncated");
    }
    return OK;
}

static RESULT M_Save(M_DEFLATE *const source)
{
    M_CHECKPOINT *const point = &source->index->points[source->index->count];
    FAIL_IF(
        inflateCopy(&point->stream, &source->stream) != Z_OK,
        "cannot save a deflate checkpoint");
    point->input_offset =
        Reader_Tell(&source->packed) - source->stream.avail_in;
    point->stream.next_in = nullptr;
    point->stream.avail_in = 0;
    point->stream.next_out = nullptr;
    point->stream.avail_out = 0;
    source->index->count++;
    return OK;
}

static RESULT M_Restore(M_DEFLATE *const source, const size_t index)
{
    if (source->initialized) {
        inflateEnd(&source->stream);
        source->initialized = false;
    }
    memset(&source->stream, 0, sizeof(source->stream));
    FAIL_IF(
        inflateCopy(&source->stream, &source->index->points[index].stream)
            != Z_OK,
        "cannot restore a deflate checkpoint");
    source->initialized = true;
    source->ended = false;
    source->position = index * M_SPAN;
    Reader_Seek(&source->packed, source->index->points[index].input_offset);
    return Reader_Result(&source->packed);
}

static RESULT M_ReadAt(
    void *const user, const size_t offset, void *const out, const size_t size)
{
    M_DEFLATE *const source = user;
    const size_t index = offset / M_SPAN;
    if (offset < source->position
        || offset - source->position > offset % M_SPAN) {
        MUST(M_Restore(source, index));
    }
    uint8_t discard[M_BUFFER_SIZE];
    while (source->position < offset) {
        const size_t count = MIN(offset - source->position, M_BUFFER_SIZE);
        size_t produced;
        MUST(M_Decode(source, discard, count, &produced));
        FAIL_IF(produced != count, "the packed data is truncated");
    }
    uint8_t *buffer = out;
    size_t remaining = size;
    while (remaining > 0) {
        const size_t count = MIN(remaining, M_BUFFER_SIZE);
        size_t produced;
        MUST(M_Decode(source, buffer, count, &produced));
        FAIL_IF(produced != count, "the packed data is truncated");
        buffer += count;
        remaining -= count;
    }
    return OK;
}

static void M_Close(void *const user)
{
    M_DEFLATE *const source = user;
    if (source->initialized) {
        inflateEnd(&source->stream);
    }
    Reader_Close(&source->packed);
    Allocator_Free(source->allocator, source, sizeof(*source));
}

static const READER_SOURCE_FUNCS m_Funcs = {
    .read_at = M_ReadAt,
    .close = M_Close,
};

static RESULT M_Index(
    M_DEFLATE *const source, const size_t size, const uint32_t expected_crc)
{
    FAIL_IF(
        inflateInit2(&source->stream, -MAX_WBITS) != Z_OK,
        "cannot start to unpack");
    source->initialized = true;
    MUST(M_Save(source));
    uint8_t buffer[M_BUFFER_SIZE];
    uint32_t crc = 0;
    while (!source->ended) {
        const size_t until_point = M_SPAN - source->position % M_SPAN;
        const size_t count =
            MIN(MIN(until_point, M_BUFFER_SIZE), size - source->position + 1);
        size_t produced;
        MUST(M_Decode(source, buffer, count, &produced));
        FAIL_IF(source->position > size, "its unpacked size does not match");
        crc = (uint32_t)crc32(crc, buffer, (uInt)produced);
        if (!source->ended && source->position < size
            && source->position % M_SPAN == 0) {
            MUST(M_Save(source));
        }
    }
    FAIL_IF(source->position != size, "its unpacked size does not match");
    FAIL_IF(
        Reader_Remaining(&source->packed) != 0 || source->stream.avail_in != 0,
        "its packed size does not match");
    FAIL_IF(crc != expected_crc, "its CRC-32 does not match");
    return OK;
}

void Deflate_FreeIndex(DEFLATE_INDEX *const index)
{
    if (index == nullptr) {
        return;
    }
    for (size_t i = 0; i < index->count; i++) {
        inflateEnd(&index->points[i].stream);
    }
    Allocator_Free(
        index->allocator, index->points,
        index->capacity * sizeof(*index->points));
    Allocator_Free(index->allocator, index, sizeof(*index));
}

RESULT Deflate_Open(
    READER *const packed, const size_t size, const uint32_t crc,
    const ALLOCATOR *const allocator, DEFLATE_INDEX **const index,
    READER *const out)
{
    Reader_OpenBuffer(out, nullptr, 0, nullptr);
    M_DEFLATE *const source = Allocator_AllocZ(allocator, sizeof(*source));
    source->packed = *packed;
    memset(packed, 0, sizeof(*packed));
    source->allocator = allocator;
    const bool build = *index == nullptr;
    source->index = *index;
    RESULT result = OK;
    if (build) {
        source->index = Allocator_AllocZ(allocator, sizeof(*source->index));
        source->index->allocator = allocator;
        source->index->capacity = size / M_SPAN + 1;
        source->index->points = Allocator_AllocZ(
            allocator,
            source->index->capacity * sizeof(*source->index->points));
        result = M_Index(source, size, crc);
    }
    if (IS_OK(result)) {
        result = M_Restore(source, 0);
    }
    if (!IS_OK(result)) {
        if (build) {
            Deflate_FreeIndex(source->index);
        }
        M_Close(source);
        return result;
    }
    *index = source->index;
    Reader_OpenSource(out, &m_Funcs, source, size, nullptr);
    return OK;
}
