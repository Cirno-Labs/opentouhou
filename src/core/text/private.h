#pragma once

#include "core/text.h"

#include <SDL3_ttf/SDL_ttf.h>

struct TEXT_FONT {
    TTF_Font *font;
    uint8_t *data;
};
