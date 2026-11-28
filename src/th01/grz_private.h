#pragma once

#include "th01/grz.h"

// Shares the HGRX decode steps between grz.c and its tests.

typedef struct {
    int32_t stream_count;
    uint16_t rle_size;
    uint16_t planar_size;
    GFX_COLOR palette[16];
    const uint8_t *rle;
    const uint8_t *planar;
} GRX_RECORD;

// Reads one HGRX header: the stream count, palette and the RLE and planar
// stream sizes that follow it.
RESULT Grz_ReadHeader(READER *reader, GRX_RECORD *record);

// Borrows the RLE and planar streams right after the header and validates
// the RLE stream covers every cell.
RESULT Grz_ReadBody(READER *reader, GRX_RECORD *record);

// Expands a validated record's RLE and planar streams into a GRX image.
void Grz_Publish(
    const GRX_RECORD *record, const ALLOCATOR *allocator, GRX *out);
