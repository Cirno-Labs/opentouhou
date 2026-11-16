#pragma once

#include "th01/bfnt.h"
#include "th01/video.h"

#define LOGO_WIDTH VIDEO_WIDTH
#define LOGO_HEIGHT VIDEO_HEIGHT

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
    VIDEO *video;
} LOGO;

// Clears both pages of an initialized VIDEO and sets the animation state.
// Keep video and its page storage alive while the logo is in use.
void Logo_Init(LOGO *logo, VIDEO *video);

// Advances one reference VSync. Each animation update spans two VSyncs.
// changed names the page drawn this tick, or -1. Returns false at the end
// of the fade. sprites must contain at least sixteen 64 by 16 patterns.
bool Logo_Tick(LOGO *logo, const BFNT *sprites, int32_t *changed);

// Converts the source palette's upper four bits through the fade intensity.
void Logo_Palette(const LOGO *logo, const BFNT *sprites, GFX_COLOR colors[16]);
