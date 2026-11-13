// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/text.h"

#include <SDL3_ttf/SDL_ttf.h>

struct TEXT_FONT {
    TTF_Font *font;
    uint8_t *data;
    // The em size for each pixel of cell height.
    float cell_scale;
    // The font that gives the characters font lacks, or nullptr.
    TEXT_FONT *fallback;
};
