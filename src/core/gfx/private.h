// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/gfx.h"

// The interface between the frontend (frontend.c), which batches quads,
// keeps the last two ticks and blends them, and a backend, which owns the
// GPU objects. A backend draws in the order of the calls it receives.

typedef enum {
    GFX_PASS_SCREEN, // the logical screen of a tick
    GFX_PASS_WINDOW, // the window, at its own resolution
} GFX_PASS;

// Maps logical-screen pixels to the pixels of a pass's target, which is
// width x height: target = logical * scale + offset. rect is the part of
// the target that shows the logical screen; the rest is border.
typedef struct {
    int32_t width, height;
    float scale_x, scale_y;
    float offset_x, offset_y;
    GFX_RECT rect;
    GFX_COLOR border;
} GFX_PASS_OUTPUT;

// Holds a view as a backend draws it. projection includes the viewport, so
// that view and then projection map a position to logical-screen pixels in
// x and y, and to depth from 0 to 1 in z. scissor is the viewport in
// logical-screen pixels.
typedef struct {
    MAT4 view;
    MAT4 projection;
    GFX_FOG fog;
    float scissor_x, scissor_y, scissor_width, scissor_height;
} GFX_PASS_VIEW;

// Holds consecutive quads that share a state, a texture and a view.
// first and count are in vertices, 4 per quad.
typedef struct {
    GFX_STATE state;
    GFX_TEXTURE *texture;
    int32_t view;
    int32_t first;
    int32_t count;
} GFX_BATCH;

struct GFX_TEXTURE {
    int32_t width, height;
    void *handle; // the backend's, nullptr without a backend
    // Links the live textures, or the freed ones that wait for the next
    // tick, because the last tick may still draw them.
    GFX_TEXTURE *next;
};

typedef struct {
    RESULT (*init)(SDL_Window *window, int32_t width, int32_t height);
    void (*shutdown)(void);
    // Creates a texture from top-down RGBA pixels, or with undefined
    // contents when pixels is nullptr.
    RESULT (*create_texture)(
        int32_t width, int32_t height, const uint8_t *pixels, void **out);
    void (*upload_texture)(void *handle, GFX_RECT rect, const IMAGE *image);
    void (*copy_from_screen)(
        void *handle, GFX_RECT source, int32_t x, int32_t y);
    void (*free_texture)(void *handle);
    // Starts drawing into the target of pass. Clears the border to
    // output->border and output->rect to clear, with depth 1.
    void (*begin_pass)(
        GFX_PASS pass, GFX_COLOR clear, const GFX_PASS_OUTPUT *output);
    // Draws batches, whose vertices are in vertices and whose views are in
    // views.
    void (*draw)(
        const GFX_PASS_VIEW *views, const GFX_BATCH *batches,
        int32_t batch_count, const GFX_VERTEX *vertices);
    // Finishes the pass. A window pass is then shown in the window.
    void (*end_pass)(void);
    // Shows the logical screen of the last screen pass in the window,
    // scaled into output->rect.
    void (*blit_screen)(const GFX_PASS_OUTPUT *output);
    // Reads the target of the last pass of the given kind.
    RESULT (*read_pixels)(
        GFX_PASS pass, const ALLOCATOR *allocator, IMAGE *out);
} GFX_BACKEND;

extern const GFX_BACKEND g_GfxBackendNull;

// Starts the frontend on backend with a logical screen of width x height,
// after backend->init succeeds. Gfx_Init calls it with the platform's
// backend; tests call it with their own.
RESULT GfxFrontend_Start(
    const GFX_BACKEND *backend, SDL_Window *window, int32_t width,
    int32_t height);
// Releases the textures waiting to be freed and stops the backend.
void GfxFrontend_Stop(void);

// Returns the id of quad index of the tick drawn last, or of the tick being
// drawn, for tests.
uint64_t GfxFrontend_GetQuadId(int32_t index);

// Returns the pass output that fits a width x height logical screen into
// output.
GFX_PASS_OUTPUT GfxOutput_Fit(
    const GFX_OUTPUT *output, int32_t width, int32_t height);
