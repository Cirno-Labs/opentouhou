#pragma once

#include <stdint.h>

// Runs full-screen fades and the play area shake. Each effect adds its
// own update element (priority 14) and, for the fades, a draw element
// (priority 16), and removes them when it ends. Drawing comes with
// graphics: the draw elements keep their place in the chain but draw
// nothing yet.

// Fades the whole screen in from color over duration frames.
void ScreenEffect_FadeIn(int32_t duration, uint32_t color);
// Moves the edges of the play area in at random each frame, by from
// pixels at the start down to to pixels at the end, over duration frames.
// The shake holds still while the game is frozen.
void ScreenEffect_Shake(int32_t duration, int32_t from, int32_t to);
// Fades the play area out to color over duration frames.
void ScreenEffect_FadeOut(int32_t duration, uint32_t color);
