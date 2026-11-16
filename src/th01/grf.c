#include "th01/grf.h"

#include <string.h>

#define M_PACKED_SIZE (GRF_WIDTH * GRF_HEIGHT / 8)

static RESULT M_DecodeRle(
    READER *const reader, const uint8_t run_byte, uint8_t packed[M_PACKED_SIZE])
{
    size_t written = 0;
    while (written < M_PACKED_SIZE) {
        uint8_t value = Reader_ReadU8(reader);
        size_t copies = 1;
        if (value == run_byte) {
            const uint8_t count = Reader_ReadU8(reader);
            if (count != 0) {
                value = Reader_ReadU8(reader);
                copies = (size_t)count + 1;
            }
        }
        MUST(Reader_Result(reader));
        const size_t remaining = M_PACKED_SIZE - written;
        const size_t amount = copies < remaining ? copies : remaining;
        memset(packed + written, value, amount);
        written += amount;
    }
    return OK;
}

RESULT Grf_Load(
    READER *const reader, const ALLOCATOR *const allocator, GRF *const out)
{
    *out = (GRF) {};
    uint8_t magic[4];
    Reader_ReadBytes(reader, magic, sizeof(magic));
    Reader_ReadU8(reader);
    const uint8_t run_byte = Reader_ReadU8(reader);
    Reader_ReadU16(reader);
    uint16_t sizes[4];
    size_t total = 0;
    for (int32_t plane = 0; plane < 4; plane++) {
        sizes[plane] = Reader_ReadU16(reader);
        total += sizes[plane];
    }
    Reader_Skip(reader, 48);
    MUST(Reader_Result(reader));
    FAIL_IF(memcmp(magic, "HGRF", 4) != 0, "Invalid GRF signature");
    FAIL_IF(total > Reader_Remaining(reader), "Truncated GRF streams");
    READER stream = Reader_Slice(reader, sizes[0]);
    MUST(Reader_Result(reader));
    uint8_t packed[M_PACKED_SIZE];
    MUST(M_DecodeRle(&stream, run_byte, packed));
    uint8_t *const coverage =
        Allocator_Alloc(allocator, GRF_WIDTH * GRF_HEIGHT);
    for (size_t byte = 0; byte < M_PACKED_SIZE; byte++) {
        for (int32_t bit = 0; bit < 8; bit++) {
            coverage[byte * 8 + bit] = (packed[byte] >> (7 - bit)) & 1;
        }
    }
    *out = (GRF) { .coverage = coverage };
    return OK;
}
