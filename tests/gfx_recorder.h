// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/gfx/private.h"

// Starts core/gfx with a backend that records what it draws, so that tests
// can check the quads of a tick and how they blend between ticks.

typedef struct {
    GFX_VERTEX vertices[4];
    GFX_STATE state;
    GFX_TEXTURE *texture;
    GFX_PASS_VIEW view;
} GFX_RECORDED_QUAD;

// Starts core/gfx on a 640 x 480 logical screen with the recording backend.
void GfxRecorder_Start(void);
void GfxRecorder_Stop(void);

// Return the quads drawn to the screen since the last Gfx_BeginTick, and
// one of them.
int32_t GfxRecorder_QuadCount(void);
const GFX_RECORDED_QUAD *GfxRecorder_Quad(int32_t index);
// Returns the id that quad index was drawn with.
uint64_t GfxRecorder_QuadId(int32_t index);
// Returns how many copies of the screen were made since GfxRecorder_Start.
int32_t GfxRecorder_CopyCount(void);
// Presents the last tick, blended toward the tick before by t, and returns
// the vertices of quad index as the window draws them.
const GFX_VERTEX *GfxRecorder_Blend(float t, int32_t index);
