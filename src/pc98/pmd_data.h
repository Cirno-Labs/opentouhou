#pragma once

#include "core/io/reader.h"

// Loads PMD 4.8 song and effect bank files into the structures the PMD
// driver plays.

#define PMD_EFFECT_COUNT 127
#define PMD_INSTRUMENT_SIZE 25

typedef struct PMD_SONG PMD_SONG;
typedef struct PMD_EFFECTS PMD_EFFECTS;

// Loads a PMD 4.8 OPN song with embedded instruments.
RESULT PmdData_LoadSong(
    READER *reader, const ALLOCATOR *allocator, PMD_SONG **out);

// Loads an OPN EFC bank; shared streams and duplicate instruments retain
// their file order. Allocator owns the bank after the reader closes.
RESULT PmdData_LoadEffects(
    READER *reader, const ALLOCATOR *allocator, PMD_EFFECTS **out);
