#pragma once

#include "th07/anm/texture.h"
#include "th07/anm/types.h"

#include "core/gfx.h"

// Identifies th07's views, so that each blends with itself between ticks.
typedef enum {
    ANM_VIEW_SCREEN = 1,
    ANM_VIEW_PLAY_AREA, // clipped to the camera viewport
    ANM_VIEW_HUD, // clipped to the camera viewport made full
    ANM_VIEW_CAMERA, // the fixed gameplay camera
    ANM_VIEW_STAGE, // the background camera
    ANM_VIEW_STAGE_ALT, // the background camera after a wrap
} ANM_VIEW;

typedef enum {
    ANM_CAMERA_FIXED,
    ANM_CAMERA_STAGE,
    ANM_CAMERA_COUNT,
} ANM_CAMERA;

typedef struct {
    GFX_VIEW view;
    GFX_DEPTH_TEST depth_test;
    bool depth_write;
    // Blends between ticks, matched by presentation generation and part.
    bool interpolate;
    // Distinguishes repeated draws of one VM within a tick.
    uint32_t part;
} ANM_DRAW;

// Returns a draw on the screen view, with unconditional depth writes.
ANM_DRAW Anm_ScreenDraw(void);

// Returns the screen view clipped to viewport, with positions in screen
// pixels.
GFX_VIEW Anm_ScreenView(VIEWPORT viewport, uint32_t id);

// Fills rect with color (0xAARRGGBB), alpha blended, without depth.
void Anm_DrawRect(GFX_RECT rect, uint32_t color);

// Sets the views of both cameras. right, the stage camera's right vector,
// sizes projected sprites.
void Anm_SetCameras(const GFX_VIEW *fixed, const GFX_VIEW *stage, VEC3 right);

// Makes camera the one that projected and 3D sprites go through.
void Anm_SetCamera(ANM_CAMERA camera);

// Records camera, or -1 for none, as the one the last 3D sprite set up. A
// 3D sprite sets up its camera only when it differs from this one.
void Anm_SetLastCamera(int32_t camera);

// Scales each channel of the colors sprites are drawn in by the channel
// of color, 0x80 for 1, up to 0xff.
void Anm_SetColorModulation(uint32_t color);

void Anm_ResetColorModulation(void);

// Moves every sprite drawn by x and y screen pixels.
void Anm_SetShake(float x, float y);

// Draws a sprite rotated about its position, as Anm_DrawUnrotated without z
// rotation. Skips hidden sprites and those outside the viewport.
void Anm_Draw(const ANM_VM *vm, const ANM_DRAW *draw);

// Draws an axis-aligned sprite with rounded screen edges.
void Anm_DrawUnrotated(const ANM_VM *vm, const ANM_DRAW *draw);

// Draws an axis-aligned sprite without pixel rounding.
void Anm_DrawNoRound(const ANM_VM *vm, const ANM_DRAW *draw);

// Draws a sprite at its 3D position projected through the current camera.
// Skips it when its projected depth is outside 0 to 1.
void Anm_DrawProjected(const ANM_VM *vm, const ANM_DRAW *draw);

// Draws the VM's quad, scaled and rotated, projected through the current
// camera.
void Anm_DrawProjected3D(ANM_VM *vm, const ANM_DRAW *draw);

// Draws the VM's quad in 3D through the camera that stage_camera picks.
void Anm_Draw3D(ANM_VM *vm, const ANM_DRAW *draw);

// Holds one vertex of a triangle strip, in screen pixels.
typedef struct {
    VEC3 pos;
    uint32_t color; // 0xAARRGGBB
    VEC2 uv;
} ANM_STRIP_VERTEX;

// Draws count vertices as a triangle strip with the texture and blending
// of vm, positions in screen pixels; skips a hidden or transparent vm.
void Anm_DrawStrip(
    const ANM_VM *vm, const ANM_STRIP_VERTEX *vertices, int32_t count,
    const ANM_DRAW *draw);
