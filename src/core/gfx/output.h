#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/gfx/types.h"
#include "core/image.h"

typedef struct SDL_Window SDL_Window;

// Starts the renderer on core/app's window, with a logical screen of
// width x height. Without Gfx_Init, every gfx call draws nothing and
// textures keep only their size.
RESULT Gfx_Init(SDL_Window *window, int32_t width, int32_t height);
void Gfx_Shutdown(void);
// Shows the last tick in the window, letterboxed, at its aspect ratio.
// With interpolate off, scales the tick's logical-screen image. With it
// on, draws the last tick again at window resolution, with positions
// blended toward the previous tick by t (0 to 1), so motion is finer than
// one logical pixel. It uses the textures as they stand after the tick: a
// texture change made between draws inside one tick can show up to one
// tick early.
RESULT Gfx_Present(bool interpolate, float t, const GFX_OUTPUT *output);
// Read back the logical screen of the last tick, or the last presented
// window, as top-down RGBA.
RESULT Gfx_ReadScreen(const ALLOCATOR *allocator, IMAGE *out);
RESULT Gfx_ReadOutput(const ALLOCATOR *allocator, IMAGE *out);
