#pragma once

#include "core/gfx/types.h"

typedef struct {
    int32_t x, y;
} ZUN_LOGO1_POINT;

// ZUN's launcher logo, as the PC-98 games open with it.
typedef struct {
    ZUN_LOGO1_POINT circles[4];
    ZUN_LOGO1_POINT velocity[4];
    ZUN_LOGO1_POINT stars[50];
    uint8_t speeds[50];
    uint8_t star_phase;
    int32_t frame, tone, sprite, period, phase, amplitude;
    uint32_t random;
    bool half_tick;
    uint8_t drawing_page, visible_page;
} ZUN_LOGO1;

// Clears the visible 640x400 area of both index pages; draws to page 0
// and shows page 1.
void ZunLogo1_Init(ZUN_LOGO1 *logo, uint8_t *pages[2]);

// Advances one VSync. Each update spans two VSyncs and flips the pages.
// sprites holds sixteen 64x16 index images, index 0 transparent. Returns
// false when the fade ends.
bool ZunLogo1_Tick(ZUN_LOGO1 *logo, uint8_t *pages[2], const uint8_t *sprites);

// Fades the upper four bits of each color by the logo's tone.
void ZunLogo1_Palette(
    const ZUN_LOGO1 *logo, const GFX_COLOR source[16], GFX_COLOR out[16]);
