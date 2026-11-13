#pragma once

#include "th06/anm/types.h"
#include "th06/anm/texture.h"

#include "core/gfx.h"

// Identifies the views that th06 draws through, so that each blends with
// itself between ticks.
typedef enum {
    ANM_VIEW_SCREEN = 1, // the whole screen
    ANM_VIEW_PLAY_AREA, // the screen, clipped to the camera viewport
    ANM_VIEW_HUD, // the screen, clipped to the camera viewport made full
    ANM_VIEW_CAMERA, // the gameplay camera
    ANM_VIEW_STAGE, // the stage background camera
} ANM_VIEW;

typedef struct {
    GFX_VIEW view;
    GFX_DEPTH_TEST depth_test;
    bool depth_write;
    // Blends the sprite between ticks, by the presentation generation of
    // the VM and part.
    bool interpolate;
    // Distinguishes repeated draws of one VM within a tick.
    uint32_t part;
} ANM_DRAW;

// Returns a draw on the screen view, with depth test and writes.
ANM_DRAW Anm_ScreenDraw(void);
// Returns the screen view clipped to viewport: positions stay in screen
// pixels, as the original's pre-transformed vertices do.
GFX_VIEW Anm_ScreenView(VIEWPORT viewport, uint32_t id);
// Draws rect of the screen in color (0xAARRGGBB), alpha blended, without
// depth.
void Anm_DrawRect(GFX_RECT rect, uint32_t color);
// Draws a sprite with pixel rounding or rotation around its rounded centre.
// Skips hidden sprites without changing the VM.
void Anm_Draw(const ANM_VM *vm, const ANM_DRAW *draw);
// Draws an axis-aligned sprite with rounded screen edges.
void Anm_DrawUnrotated(const ANM_VM *vm, const ANM_DRAW *draw);
// Draws an axis-aligned sprite without pixel rounding.
void Anm_DrawNoRound(const ANM_VM *vm, const ANM_DRAW *draw);
// Draws the fixed world quad with the sprite scaling and XYZ rotations.
// The quad is 256 units wide for a sprite as wide as its texture, as in
// the game. With nmlgc/base_tsa, the quad is as many units wide and high
// as the sprite has pixels, as its executable patches make it, so sprites
// from textures larger than 256 pixels keep their size.
void Anm_Draw3D(const ANM_VM *vm, const ANM_DRAW *draw);
// Rounds world translation when no rotation is set.
void Anm_Draw3DRounded(const ANM_VM *vm, const ANM_DRAW *draw);
