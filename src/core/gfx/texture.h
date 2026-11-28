#pragma once

#include "core/base/result.h"
#include "core/gfx/types.h"
#include "core/image.h"

// Holds pixels on the GPU for drawing. A texture is RGBA or indexed with
// a palette; both upload and sample the same way through draw.h.
//
// Creates a texture holding image. Headless, keeps only the size.

RESULT GfxTexture_Create(const IMAGE *image, GFX_TEXTURE **out);

// Creates an indexed texture with one byte per pixel and 256 RGBA palette
// entries. Initializes indices and palette entries to zero. Indexed textures
// always sample indices with nearest filtering.
RESULT GfxTexture_CreateIndexed(
    int32_t width, int32_t height, GFX_TEXTURE **out);

// Replaces rect with tightly packed, top-down indices. Updates take effect
// in call order, as with GfxTexture_Upload.
void GfxTexture_UploadIndices(
    GFX_TEXTURE *texture, GFX_RECT rect, const uint8_t *indices);

// Replaces count palette entries starting at first. Earlier draws in the
// tick keep the old palette. Alpha comes from the palette entry.
void GfxTexture_UploadPalette(
    GFX_TEXTURE *texture, int32_t first, int32_t count,
    const GFX_COLOR *colors);

// Creates a texture of the given size with undefined contents.
RESULT GfxTexture_CreateEmpty(int32_t width, int32_t height, GFX_TEXTURE **out);
int32_t GfxTexture_GetWidth(const GFX_TEXTURE *texture);
int32_t GfxTexture_GetHeight(const GFX_TEXTURE *texture);

// Replaces rect of an RGBA texture with image, which has the size of rect.
// Takes effect in call order: quads drawn earlier in the tick keep the old
// contents.
void GfxTexture_Upload(GFX_TEXTURE *texture, GFX_RECT rect, const IMAGE *image);

// Copies source of the logical screen, as drawn so far in this tick, to
// x, y of an RGBA texture.
void GfxTexture_CopyFromScreen(
    GFX_TEXTURE *texture, GFX_RECT source, int32_t x, int32_t y);
void GfxTexture_Free(GFX_TEXTURE *texture);
