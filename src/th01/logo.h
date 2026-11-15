#pragma once

#include "th01/bfnt.h"

#define LOGO_WIDTH 640
#define LOGO_HEIGHT 400

typedef struct {
    int32_t x, y;
} LOGO_POINT;

typedef struct {
    LOGO_POINT circles[4];
    LOGO_POINT velocity[4];
    LOGO_POINT stars[50];
    uint8_t speeds[50];
    uint8_t star_phase;
    int32_t frame, tone, sprite, period, phase, amplitude;
    uint32_t random;
    bool half_tick;
    int32_t drawing_page, visible_page;
    uint8_t pages[2][LOGO_WIDTH * LOGO_HEIGHT];
} LOGO;

// Initializes the two graphics pages and the deterministic animation state.
void Logo_Init(LOGO *logo);

// Advances one 60 Hz shell tick. Each animation update spans two ticks.
// changed names the page drawn this tick, or -1. Returns false at the end
// of the fade. sprites must contain at least sixteen 64 by 16 patterns.
bool Logo_Tick(LOGO *logo, const BFNT *sprites, int32_t *changed);

// Converts the source palette's upper four bits through the fade intensity.
void Logo_Palette(const LOGO *logo, const BFNT *sprites, GFX_COLOR colors[16]);
