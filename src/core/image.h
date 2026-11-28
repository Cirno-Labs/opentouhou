#pragma once

// Loads, holds and saves pixels in memory, decoupled from the renderer:
// gfx/texture.h uploads an IMAGE to a GFX_TEXTURE when it is ready to
// draw.

#include "core/image/decode.h"
#include "core/image/encode.h"
#include "core/image/pixels.h"
#include "core/image/resize.h"
