#pragma once

#include "th06/anm/types.h"

#include "core/gfx.h"
#include "core/image.h"

#define ANM_TEXTURE_COUNT 0x108
#define ANM_SURFACE_COUNT 0x20

typedef enum {
    ANM_TEXTURE_DEFAULT,
    ANM_TEXTURE_RGBA8,
    ANM_TEXTURE_A1R5G5B5,
    ANM_TEXTURE_R5G6B5,
    ANM_TEXTURE_RGB8,
    ANM_TEXTURE_A4R4G4B4,
} ANM_TEXTURE_FORMAT;

RESULT Anm_LoadTexture(
    int32_t slot, const char *path, ANM_TEXTURE_FORMAT format,
    uint32_t color_key);
RESULT Anm_LoadTextureAlpha(
    int32_t slot, const char *path, ANM_TEXTURE_FORMAT format,
    uint32_t color_key);
RESULT Anm_CreateTexture(
    int32_t slot, int32_t width, int32_t height, ANM_TEXTURE_FORMAT format);
void Anm_ReleaseTexture(int32_t slot);
// Returns the texture without transferring ownership.
GFX_TEXTURE *Anm_GetTexture(int32_t slot);
// Replaces a texture rectangle with image pixels.
RESULT Anm_UpdateTexture(int32_t slot, GFX_RECT rectangle, const IMAGE *image);

RESULT Anm_LoadSurface(int32_t surface, const char *path);
void Anm_ReleaseSurface(int32_t surface);
// Draw the surface, or rectangle of it, at x, y of the screen, unblended,
// as the original's CopyRects does.
void Anm_CopySurfaceToBackBuffer(
    int32_t surface, int32_t left, int32_t top, int32_t x, int32_t y);
void Anm_CopySurfaceRectToBackBuffer(
    int32_t surface, int32_t x, int32_t y, GFX_RECT rectangle);

// Replaces the texture of slot with a copy of rectangle of the screen as
// drawn so far. Sprites that cover the whole texture show the whole
// rectangle, as the original's stretching copy does.
RESULT Anm_CopyBackBufferToTexture(int32_t slot, GFX_RECT rectangle);
// Requests one copy after the current draw and update chains finish.
void Anm_SetPendingCopy(int32_t slot, GFX_RECT rectangle);
// Makes the requested copy.
RESULT Anm_DoPendingCopy(void);
