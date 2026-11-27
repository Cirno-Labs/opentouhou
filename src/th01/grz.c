#include "th01/grz_private.h"

#include <string.h>

#define M_CELL_COUNT (GRX_WIDTH * GRX_HEIGHT / 8)
#define M_PIXEL_COUNT (GRX_WIDTH * GRX_HEIGHT)
#define M_GRZ_HEADER_SIZE 96
#define M_GRX_HEADER_SIZE 64

RESULT Grz_ReadHeader(READER *const reader, GRX_RECORD *const record)
{
    uint8_t magic[4];
    Reader_ReadBytes(reader, magic, sizeof(magic));
    record->stream_count = Reader_ReadU8(reader);
    Reader_Skip(reader, 3);
    record->rle_size = Reader_ReadU16(reader);
    record->planar_size = Reader_ReadU16(reader);
    Reader_Skip(reader, 4);
    MUST(Reader_Result(reader));
    FAIL_IF(memcmp(magic, "HGRX", 4) != 0, "Invalid GRX signature");
    FAIL_IF(
        record->stream_count < 1 || record->stream_count > GRX_MAX_STREAMS,
        "Invalid GRX stream count");
    for (int32_t i = 0; i < 16; i++) {
        const uint8_t red = Reader_ReadU8(reader);
        const uint8_t green = Reader_ReadU8(reader);
        const uint8_t blue = Reader_ReadU8(reader);
        MUST(Reader_Result(reader));
        FAIL_IF(
            red > 15 || green > 15 || blue > 15,
            "Invalid GRX palette component");
        record->palette[i] =
            (GFX_COLOR) { red * 17, green * 17, blue * 17, 255 };
    }
    return OK;
}

static void M_ReadRun(
    READER *const reader, size_t *const cells, bool *const put)
{
    uint8_t command = Reader_ReadU8(reader);
    *cells = 1;
    if (command == 1) {
        const uint8_t count = Reader_ReadU8(reader);
        if (count != 0) {
            command = Reader_ReadU8(reader);
            *cells += count;
        }
    }
    *put = command == 0;
}

static RESULT M_Validate(const GRX_RECORD *const record)
{
    READER rle;
    Reader_OpenBuffer(&rle, record->rle, record->rle_size, nullptr);
    size_t cell = 0;
    size_t puts = 0;
    while (cell < M_CELL_COUNT) {
        size_t cells;
        bool put;
        M_ReadRun(&rle, &cells, &put);
        MUST(Reader_Result(&rle));
        const size_t remaining = M_CELL_COUNT - cell;
        const size_t count = cells < remaining ? cells : remaining;
        if (put) {
            puts += count;
        }
        cell += count;
    }
    FAIL_IF(puts > record->planar_size / 4, "Truncated GRX planar stream");
    return OK;
}

RESULT Grz_ReadBody(READER *const reader, GRX_RECORD *const record)
{
    record->rle = Reader_Borrow(reader, record->rle_size);
    record->planar = Reader_Borrow(
        reader, (size_t)record->stream_count * record->planar_size);
    MUST(Reader_Result(reader));
    return M_Validate(record);
}

void Grz_Publish(
    const GRX_RECORD *const record, const ALLOCATOR *const allocator,
    GRX *const out)
{
    GRX image = {
        .stream_count = record->stream_count,
        .coverage = Allocator_AllocZ(allocator, M_PIXEL_COUNT),
        .pixels = Allocator_AllocZ(
            allocator, (size_t)record->stream_count * M_PIXEL_COUNT),
    };
    memcpy(image.palette, record->palette, sizeof(image.palette));
    READER rle;
    Reader_OpenBuffer(&rle, record->rle, record->rle_size, nullptr);
    size_t cell = 0;
    size_t planar_cell = 0;
    while (cell < M_CELL_COUNT) {
        size_t cells;
        bool put;
        M_ReadRun(&rle, &cells, &put);
        const size_t remaining = M_CELL_COUNT - cell;
        const size_t count = cells < remaining ? cells : remaining;
        if (put) {
            memset(image.coverage + cell * 8, 1, count * 8);
            for (int32_t stream = 0; stream < record->stream_count; stream++) {
                const uint8_t *planes = record->planar
                    + (size_t)stream * record->planar_size + planar_cell * 4;
                uint8_t *pixels =
                    image.pixels + (size_t)stream * M_PIXEL_COUNT + cell * 8;
                for (size_t i = 0; i < count; i++) {
                    for (int32_t bit = 0; bit < 8; bit++) {
                        uint8_t color = 0;
                        for (int32_t plane = 0; plane < 4; plane++) {
                            color |= ((planes[plane] >> (7 - bit)) & 1)
                                << plane;
                        }
                        pixels[bit] = color;
                    }
                    planes += 4;
                    pixels += 8;
                }
            }
            planar_cell += count;
        }
        cell += count;
    }
    *out = image;
}

RESULT Grz_Load(
    READER *const reader, const ALLOCATOR *const allocator, GRZ *const out)
{
    *out = (GRZ) {};
    const size_t start = Reader_Tell(reader);
    const size_t available = Reader_Remaining(reader);
    uint8_t magic[4];
    Reader_ReadBytes(reader, magic, sizeof(magic));
    const int32_t count = Reader_ReadU8(reader);
    Reader_Skip(reader, 3);
    int32_t offsets[GRZ_MAX_IMAGES];
    for (int32_t i = 0; i < GRZ_MAX_IMAGES; i++) {
        offsets[i] = Reader_ReadI32(reader);
    }
    const int32_t total = Reader_ReadI32(reader);
    Reader_Skip(reader, 20);
    MUST(Reader_Result(reader));
    FAIL_IF(memcmp(magic, "HGRZ", 4) != 0, "Invalid GRZ signature");
    FAIL_IF(count < 1 || count > GRZ_MAX_IMAGES, "Invalid GRZ image count");
    FAIL_IF(
        total < M_GRZ_HEADER_SIZE || (size_t)total > available,
        "Invalid GRZ recorded size");
    for (int32_t i = 0; i < count; i++) {
        FAIL_IF(
            offsets[i] < M_GRZ_HEADER_SIZE
                || offsets[i] > total - M_GRX_HEADER_SIZE,
            "Invalid GRZ image offset");
    }
    GRX_RECORD records[GRZ_MAX_IMAGES] = {};
    size_t sizes[GRZ_MAX_IMAGES];
    size_t working_size = 0;
    for (int32_t i = 0; i < count; i++) {
        Reader_Seek(reader, start + offsets[i]);
        MUST(Grz_ReadHeader(reader, &records[i]));
        sizes[i] = records[i].rle_size
            + (size_t)records[i].stream_count * records[i].planar_size;
        FAIL_IF(
            sizes[i] > (size_t)(total - offsets[i] - M_GRX_HEADER_SIZE),
            "Truncated GRZ image");
        working_size += sizes[i];
    }
    uint8_t *const data = Mem_Alloc(working_size);
    size_t position = 0;
    RESULT result = OK;
    for (int32_t i = 0; i < count && IS_OK(result); i++) {
        Reader_Seek(reader, start + offsets[i] + M_GRX_HEADER_SIZE);
        Reader_ReadBytes(reader, data + position, sizes[i]);
        result = Reader_Result(reader);
        if (IS_OK(result)) {
            READER body;
            Reader_OpenBuffer(&body, data + position, sizes[i], nullptr);
            result = Grz_ReadBody(&body, &records[i]);
        }
        position += sizes[i];
    }
    if (IS_OK(result)) {
        Reader_Seek(reader, start + total);
        result = Reader_Result(reader);
    }
    if (IS_OK(result)) {
        GRZ images = { .count = count };
        for (int32_t i = 0; i < count; i++) {
            Grz_Publish(&records[i], allocator, &images.images[i]);
        }
        *out = images;
    }
    Mem_Free(data);
    return result;
}
