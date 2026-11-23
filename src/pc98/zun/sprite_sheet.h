#pragma once

#include "core/vfs.h"

// Loads the one-bit BMP name from vfs as cels of cel_width by cel_height
// pixels, left to right, then top to bottom, each row packed most
// significant bit first, as upstream's bmp2arr lays out a sheet. A white
// pixel is a set bit. cel_width must be a multiple of 8; *out holds *size
// bytes from allocator.
RESULT SpriteSheet_Load(
    VFS *vfs, const char *name, int32_t cel_width, int32_t cel_height,
    const ALLOCATOR *allocator, uint8_t **out, size_t *size);
