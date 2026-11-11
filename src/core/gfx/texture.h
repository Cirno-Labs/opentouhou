#pragma once

#include "core/base/result.h"
#include "core/gfx/types.h"
#include "core/image.h"

// Creates a texture holding image. Headless, keeps only the size.
RESULT GfxTexture_Create(const IMAGE *image, GFX_TEXTURE **out);
// Creates a texture of the given size with undefined contents.
RESULT GfxTexture_CreateEmpty(int32_t width, int32_t height, GFX_TEXTURE **out);
int32_t GfxTexture_GetWidth(const GFX_TEXTURE *texture);
int32_t GfxTexture_GetHeight(const GFX_TEXTURE *texture);
// Replaces rect with image, which has the size of rect. Takes effect in
// call order: quads drawn earlier in the tick keep the old contents.
void GfxTexture_Upload(GFX_TEXTURE *texture, GFX_RECT rect, const IMAGE *image);
// Copies source of the logical screen, as drawn so far in this tick, to
// x, y of the texture.
void GfxTexture_CopyFromScreen(
    GFX_TEXTURE *texture, GFX_RECT source, int32_t x, int32_t y);
void GfxTexture_Free(GFX_TEXTURE *texture);
