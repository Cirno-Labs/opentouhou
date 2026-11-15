#pragma once

#include "th07/anm/types.h"

#include "core/gfx.h"
#include "core/image.h"
#include "core/io/reader.h"

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

// Creates the texture of slot from the image an ANM file stores at reader's
// position, quantized to format.
RESULT Anm_CreateTextureFromImage(
    int32_t slot, READER *reader, ANM_TEXTURE_FORMAT format);
void Anm_ReleaseTexture(int32_t slot);
GFX_TEXTURE *Anm_GetTexture(int32_t slot);

// Replaces a texture rectangle with pixels quantized to its loaded format.
RESULT Anm_UpdateTexture(int32_t slot, GFX_RECT rectangle, const IMAGE *image);

// Copies source of a texture, scaled to dest of another, through their kept
// pixels. Does nothing while either is not loaded.
RESULT Anm_CopyTextureRect(
    int32_t dest_slot, GFX_RECT dest, int32_t source_slot, GFX_RECT source);

// Requests a copy of source of the screen, scaled to dest of the texture of
// slot, after the draw and update chains. Returns false while a copy is
// pending.
bool Anm_SetPendingCopy(int32_t slot, GFX_RECT source, GFX_RECT dest);

RESULT Anm_LoadSurface(int32_t surface, const char *path);
void Anm_ReleaseSurface(int32_t surface);

// Draws the surface from left, top at x, y of the screen, unblended, as
// the original's CopyRects does.
void Anm_CopySurfaceToBackBuffer(
    int32_t surface, int32_t left, int32_t top, int32_t x, int32_t y);
void Anm_CopySurfaceRectToBackBuffer(
    int32_t surface, int32_t x, int32_t y, GFX_RECT rectangle);
