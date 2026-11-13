#pragma once

#include "core/gfx/types.h"

// Returns the view of the logical screen in its own pixels: x right,
// y down, z from 0 to 1, pixel centres at half integers. Without
// Gfx_Init, returns identity matrices and an empty viewport.
GFX_VIEW GfxView_Screen(void);

// Starts drawing a tick on the logical screen, cleared to clear. The program
// calls Gfx_BeginTick and Gfx_EndTick around each tick of the game.
void Gfx_BeginTick(GFX_COLOR clear);
// Sets the view of the quads that follow. The first view of a tick is
// GfxView_Screen().
void Gfx_SetView(const GFX_VIEW *view);
// Draws a quad, vertices in perimeter order, without a texture when
// texture is nullptr. id identifies the quad across ticks and must be
// unique within one tick; 0 draws it without blending. A duplicate id is
// fatal in debug builds; in release builds one of the quads blends against
// the wrong one for a frame. Interpolation blends vertex positions.
void Gfx_DrawQuad(
    const GFX_STATE *state, GFX_TEXTURE *texture, const GFX_VERTEX vertices[4],
    uint64_t id);
// Draws a quad as Gfx_DrawQuad does. Interpolation blends center, scale
// and rotation of motion instead of the vertex positions, and moves the
// exact vertices with them, so a rotating sprite keeps its shape.
// Rotation takes the shorter way round.
void Gfx_DrawSprite(
    const GFX_STATE *state, GFX_TEXTURE *texture, const GFX_VERTEX vertices[4],
    const GFX_MOTION *motion, uint64_t id);
// Finishes the tick's drawing of the logical screen.
void Gfx_EndTick(void);
