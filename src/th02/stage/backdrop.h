#pragma once

#include "pc98/zun/pi.h"
#include "th02/draw.h"
#include "th02/palette.h"

#include "core/vfs.h"

typedef struct {
    PI_IMAGE image;
    ALLOCATOR allocator;
} STAGE_BACKDROP;

// Loads the backdrop; the allocator must free individually.
RESULT StageBackdrop_Load(
    VFS *files, const ALLOCATOR *allocator, STAGE_BACKDROP *out);

// Draws the backdrop and applies its palette, ignoring the file's position
// and color key. Requires disabled charging.
void StageBackdrop_Draw(const STAGE_BACKDROP *backdrop);

// Frees the pixels once nothing borrows them; accepts zeroed storage.
void StageBackdrop_Free(STAGE_BACKDROP *backdrop);
