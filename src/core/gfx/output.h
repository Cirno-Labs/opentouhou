#pragma once

#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/gfx/types.h"
#include "core/image.h"

// Owns the renderer and the window it draws into: starting and stopping
// it, and presenting the finished tick's image to the screen.

typedef struct SDL_Window SDL_Window;

// Starts the renderer on the program's window, with a logical screen of
// width x height. Without Gfx_Init, every gfx call draws nothing and
// textures keep only their size.
RESULT Gfx_Init(SDL_Window *window, int32_t width, int32_t height);
void Gfx_Shutdown(void);

// Returns whether the renderer is started.
bool Gfx_IsStarted(void);

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

// Reads back source of the logical screen as drawn so far in this tick,
// as top-down RGBA. A source outside the screen, or a renderer that is not
// started, is a failure.
RESULT Gfx_ReadScreenRect(
    GFX_RECT source, const ALLOCATOR *allocator, IMAGE *out);
