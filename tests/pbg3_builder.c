#include "pbg3_builder.h"

#include <string.h>

#define M_HEADER_VARINT_BYTES 4

static void M_WriteBits(
    PBG3_BUILDER_WRITER *const writer, const uint32_t value,
    const int32_t count)
{
    for (int32_t i = count - 1; i >= 0; i--) {
        writer->current = (writer->current << 1) | ((value >> i) & 1);
        writer->bits++;
        if (writer->bits == 8) {
            writer->bytes[writer->length++] = (uint8_t)writer->current;
            writer->current = 0;
            writer->bits = 0;
        }
    }
}

static void M_WriteVarintN(
    PBG3_BUILDER_WRITER *const writer, const uint32_t value,
    const int32_t byte_count)
{
    M_WriteBits(writer, (uint32_t)(byte_count - 1), 2);
    M_WriteBits(writer, value, byte_count * 8);
}

static void M_WriteVarint(
    PBG3_BUILDER_WRITER *const writer, const uint32_t value)
{
    int32_t byte_count = 1;
    while (byte_count < 4 && (value >> (byte_count * 8)) != 0) {
        byte_count++;
    }
    M_WriteVarintN(writer, value, byte_count);
}

static void M_WriteName(
    PBG3_BUILDER_WRITER *const writer, const char *const name)
{
    for (const unsigned char *c = (const unsigned char *)name; *c != '\0';
         c++) {
        M_WriteBits(writer, *c, 8);
    }
    M_WriteBits(writer, 0, 8);
}

void Pbg3Builder_Init(PBG3_BUILDER_WRITER *const writer)
{
    writer->length = 0;
    writer->current = 0;
    writer->bits = 0;
}

void Pbg3Builder_WriteLiteral(
    PBG3_BUILDER_WRITER *const writer, const uint8_t byte)
{
    M_WriteBits(writer, 1, 1);
    M_WriteBits(writer, byte, 8);
}

void Pbg3Builder_WriteBackref(
    PBG3_BUILDER_WRITER *const writer, const uint32_t offset,
    const uint32_t length)
{
    M_WriteBits(writer, 0, 1);
    M_WriteBits(writer, offset, 13);
    M_WriteBits(writer, length - 3, 4);
}

void Pbg3Builder_WriteStop(PBG3_BUILDER_WRITER *const writer)
{
    M_WriteBits(writer, 0, 1);
    M_WriteBits(writer, 0, 13);
}

void Pbg3Builder_FlushByte(PBG3_BUILDER_WRITER *const writer)
{
    if (writer->bits > 0) {
        writer->bytes[writer->length++] =
            (uint8_t)(writer->current << (8 - writer->bits));
        writer->current = 0;
        writer->bits = 0;
    }
}

uint32_t Pbg3Builder_Checksum(const uint8_t *const bytes, const size_t size)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < size; i++) {
        sum += bytes[i];
    }
    return sum;
}

size_t Pbg3Builder_BuildArchive(
    const PBG3_BUILDER_ENTRY *const entries, const int32_t count,
    uint8_t *const out)
{
    const size_t header_bits = (2 + M_HEADER_VARINT_BYTES * 8) * 2;
    const size_t header_size = (header_bits + 7) / 8;
    const size_t data_start = 4 + header_size;

    size_t offsets[64];
    size_t cursor = data_start;
    for (int32_t i = 0; i < count; i++) {
        offsets[i] = cursor;
        cursor += entries[i].compressed_size;
    }
    const uint32_t table_offset = (uint32_t)cursor;

    memcpy(out, "PBG3", 4);
    PBG3_BUILDER_WRITER header;
    Pbg3Builder_Init(&header);
    M_WriteVarintN(&header, (uint32_t)count, M_HEADER_VARINT_BYTES);
    M_WriteVarintN(&header, table_offset, M_HEADER_VARINT_BYTES);
    Pbg3Builder_FlushByte(&header);
    memcpy(out + 4, header.bytes, header.length);

    for (int32_t i = 0; i < count; i++) {
        memcpy(
            out + offsets[i], entries[i].compressed,
            entries[i].compressed_size);
    }

    PBG3_BUILDER_WRITER table;
    Pbg3Builder_Init(&table);
    for (int32_t i = 0; i < count; i++) {
        M_WriteVarint(&table, 0); // unk1
        M_WriteVarint(&table, 0); // unk0
        M_WriteVarint(&table, entries[i].checksum);
        M_WriteVarint(&table, (uint32_t)offsets[i]);
        M_WriteVarint(&table, entries[i].decoded_size);
        M_WriteName(&table, entries[i].name);
    }
    Pbg3Builder_FlushByte(&table);
    memcpy(out + table_offset, table.bytes, table.length);

    return table_offset + table.length;
}
