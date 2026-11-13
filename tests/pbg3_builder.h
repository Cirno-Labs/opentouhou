// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <stddef.h>
#include <stdint.h>

// Builds a PBG3 archive byte for byte, to check a reader against bytes
// whose layout is known, instead of trusting a second implementation of
// the format.

#define PBG3_BUILDER_MAX_ARCHIVE_SIZE 65536

typedef struct {
    uint8_t bytes[PBG3_BUILDER_MAX_ARCHIVE_SIZE];
    size_t length;
    uint32_t current;
    int32_t bits;
} PBG3_BUILDER_WRITER;

typedef struct {
    const char *name;
    const uint8_t *compressed;
    size_t compressed_size;
    uint32_t checksum;
    uint32_t decoded_size;
} PBG3_BUILDER_ENTRY;

// Clears writer to an empty bitstream.
void Pbg3Builder_Init(PBG3_BUILDER_WRITER *writer);
// Appends one literal byte.
void Pbg3Builder_WriteLiteral(PBG3_BUILDER_WRITER *writer, uint8_t byte);
// Appends a back-reference copying length bytes (3 to 18) starting at
// dict index offset. dict[k] holds the (k - 1)th byte the stream has
// produced, so offset is one more than the index of the byte to repeat.
void Pbg3Builder_WriteBackref(
    PBG3_BUILDER_WRITER *writer, uint32_t offset, uint32_t length);
// Appends the stream's stop code.
void Pbg3Builder_WriteStop(PBG3_BUILDER_WRITER *writer);
// Pads the last partial byte with zero bits, if any.
void Pbg3Builder_FlushByte(PBG3_BUILDER_WRITER *writer);

// Returns the sum of bytes, as the table's entry checksum is computed.
uint32_t Pbg3Builder_Checksum(const uint8_t *bytes, size_t size);

// Assembles a whole PBG3 archive from already-compressed entries into out:
// the magic, a fixed-width header, each entry's compressed bytes back to
// back, and the table. Returns the number of bytes written.
size_t Pbg3Builder_BuildArchive(
    const PBG3_BUILDER_ENTRY *entries, int32_t count, uint8_t *out);
