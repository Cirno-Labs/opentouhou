#pragma once

// What the draw parts share: the raw sprite blits, which the public sprite
// draws wrap with the charger state the original leaves behind.

#include "th02/clip.h"
#include "th02/grcg.h"
#include "th02/patterns.h"
#include "th02/video.h"

// Draws a sprite without clipping; as in retail, X is unsigned and
// addresses wrap within the planes.
void Blit_Put(const PATTERN *pattern, int16_t left, int16_t top);

// Draws a sprite with rows wrapping at 400, negative Y included.
void Blit_RollPut(const PATTERN *pattern, int16_t left, int16_t top);

// Draws a sprite clipped to clip and reports whether it drew; as in retail,
// signed-word overflow can take the unclipped path.
bool Blit_PutRect(
    const PATTERN *pattern, const CLIP *clip, int16_t left, int16_t top);

// Draws a tiny sprite with rows wrapping at 400; when X carries, it can
// wrap a row earlier than RollPut.
void Blit_RollPutTiny(const PATTERN *pattern, int16_t left, int16_t top);

// Draws source plane 0..4 as coverage; mode's low bits disable planes and
// its high byte is their tile.
void Blit_PutPlane(
    const PATTERN *pattern, int16_t left, int16_t top, uint8_t source_plane,
    uint16_t mode);
void Blit_RollPutPlane(
    const PATTERN *pattern, int16_t left, int16_t top, uint8_t source_plane,
    uint16_t mode);

// Draws a sprite scaled and clipped, ignoring coverage and color zero;
// nonpositive scales reverse the box.
void Blit_Zoom(
    const PATTERN *pattern, const CLIP *clip, int16_t left, int16_t top,
    int16_t scale);

// Draws a sprite opaquely for the ending; as in retail, B and R planes
// alias into R and G.
void Blit_OverPut8(const PATTERN *pattern, int16_t left, int16_t top);
