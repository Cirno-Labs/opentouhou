#pragma once

// Pattern banks: the numbered slots of sprite images that the game loads
// from BFNT files and its blitters draw by number. The tiny format packs
// small images of up to four colors for the tiny blitters.

#include "pc98/zun/bfnt.h"

#define PATTERN_LIMIT 512

typedef struct {
    uint16_t size;
    // Marks the first pattern appended from a BFNT file.
    bool sheet_start;
    uint8_t *data;
    size_t storage_size;
} PATTERN;

typedef struct {
    ALLOCATOR allocator;
    uint16_t next;
    PATTERN entries[PATTERN_LIMIT];
} PATTERN_BANK;

// Initializes an empty bank; Reset it before initializing it again.
void PatternBank_Init(PATTERN_BANK *bank, const ALLOCATOR *allocator);
void PatternBank_Reset(PATTERN_BANK *bank);

// Adds a pattern of width a multiple of eight, sizes up to 255 and
// five-plane size up to 65535; failure leaves the bank unchanged.
RESULT PatternBank_Put(
    PATTERN_BANK *bank, uint16_t slot, int32_t width, int32_t height,
    const uint8_t *pixels, uint8_t key);

// Appends a BFNT's images; when the bank fills, those already added stay.
RESULT PatternBank_AppendBfnt(PATTERN_BANK *bank, const BFNT *sprites);

// Converts a slot of even plane size and up to four colors to the tiny
// format, which ordinary blitters cannot draw.
RESULT PatternBank_ConvertTiny(PATTERN_BANK *bank, uint16_t slot);

// Moves the cursor to 0..PATTERN_LIMIT, clearing no slots.
RESULT PatternBank_SetNext(PATTERN_BANK *bank, uint16_t next);

// Removes a slot; removing the last one also moves the cursor back past
// empty slots.
RESULT PatternBank_Cancel(PATTERN_BANK *bank, uint16_t slot);

// Frees the slots first..last of a nonempty bank, skipping absent ones.
void PatternBank_Clean(PATTERN_BANK *bank, uint16_t first, uint16_t last);
