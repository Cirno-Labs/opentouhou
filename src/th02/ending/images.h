#pragma once

#include "th02/ending/assets.h"
#include "th02/ending/ending.h"
#include "th02/input.h"
#include "th02/palette.h"
#include "th02/session.h"

// Loads the ending images with up to 48 palette bytes, short files
// included.
RESULT EndingImages_Load(
    VFS *files, const ALLOCATOR *allocator, ENDING_IMAGES *out);

// Draws an image, ignoring its file position, color key and X clipping.
// Requires disabled charging.
void EndingImages_Draw(const ENDING_IMAGES *images, ENDING_IMAGE image);

// Shows the image's palette at the live tone.
void EndingImages_ApplyPalette(const ENDING_IMAGES *images, ENDING_IMAGE image);

// Recolors with Palette_ReadRgb's partial reads and nibble expansion.
void EndingImages_Recolor(const ENDING_IMAGES *images, ENDING_COLOR color);

// Frees the images once nothing borrows them; accepts zeroed storage.
void EndingImages_Free(ENDING_IMAGES *images);
