#pragma once

#include "th02/draw.h"
#include "th02/mpn.h"
#include "th02/tram.h"

#include "core/vfs.h"

typedef struct {
    TRAM_TEXT lines[2];
} DIALOGUE_BOX;

typedef struct {
    ALLOCATOR allocator;
    DIALOGUE_BOX *boxes;
    uint8_t count;
    TRAM_TEXT blank;
    MPN faces;
} DIALOGUE_ASSETS;

typedef struct {
    uint8_t counts[12];
    uint8_t faces[12][22];
    uint8_t stage4_intro[8];
    uint8_t stage4_marisa[11];
    uint8_t stage4_before[3];
    uint8_t stage4_after[5];
    uint8_t stage4_no_continue[5];
    uint8_t stage5_continued[22];
    uint8_t stage5_no_continue[17];
    uint8_t stage5_form_continued[5];
    uint8_t stage5_form_no_continue[4];
    TRAM_CHARACTER digits[10];
} DIALOGUE_SCRIPTS;

// Loads up to 64 80-byte boxes; the allocator must free individually.
RESULT DialogueAssets_Load(
    VFS *files, uint8_t stage, const ALLOCATOR *allocator,
    DIALOGUE_ASSETS *out);

// Frees the boxes once nothing draws them; accepts zeroed storage.
void DialogueAssets_Free(DIALOGUE_ASSETS *assets);

// Loads the static dialogue tables.
RESULT DialogueScripts_Load(DIALOGUE_SCRIPTS *out);
