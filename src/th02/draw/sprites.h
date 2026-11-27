#pragma once

// Sprite draws: pattern-bank images put into the planes the ways the game
// draws them, each leaving the charger as the original leaves it.

#include "th02/patterns.h"

// Draws a sprite without clipping, X unsigned and wrapping within the
// planes, then leaves the charger off with all tiles set.
void Draw_Sprite(const PATTERN *pattern, int16_t left, int16_t top);

// Draws a sprite with rows wrapping at 400, negative Y included, then
// leaves the charger off with all tiles set.
void Draw_SpriteRoll(const PATTERN *pattern, int16_t left, int16_t top);

// Draws a sprite clipped to the clip bounds; when it drew, leaves the
// charger off with all tiles set.
void Draw_SpriteRect(const PATTERN *pattern, int16_t left, int16_t top);

// Draws a tiny sprite with rows wrapping at 400, replaying the color
// switches of its records on the charger.
void Draw_SpriteRollTiny(const PATTERN *pattern, int16_t left, int16_t top);

// Draws source plane 0..4 as coverage; mode's low bits disable planes and
// its high byte is their tile, which the charger keeps afterwards.
void Draw_SpritePlane(
    const PATTERN *pattern, int16_t left, int16_t top, uint8_t source_plane,
    uint16_t mode);
void Draw_SpriteRollPlane(
    const PATTERN *pattern, int16_t left, int16_t top, uint8_t source_plane,
    uint16_t mode);

// Draws a sprite scaled and clipped to the clip bounds, ignoring coverage
// and color zero; nonpositive scales reverse the box.
void Draw_SpriteZoom(
    const PATTERN *pattern, int16_t left, int16_t top, int16_t scale);

// Draws a sprite opaquely for the ending, the B and R planes aliasing into
// R and G; requires charging off.
void Draw_SpriteOpaque(const PATTERN *pattern, int16_t left, int16_t top);

// Writes color to the pixels of mask at a VRAM byte offset of the drawing
// page, bypassing the charger.
void Draw_MaskByte(uint32_t offset, uint8_t mask, uint8_t color);
