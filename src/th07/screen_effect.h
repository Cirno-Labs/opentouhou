#pragma once

#include <stdint.h>

// Runs full-screen fades, play area flashes and the sprite shake. Each
// effect adds its own update element (priority 15) and, but for the
// shake, a draw element (priority 17), and removes them when it ends.
// Timers run at the game speed.

// Fades the whole screen in from color over duration frames.
void ScreenEffect_FadeIn(int32_t duration, uint32_t color);

// Moves every sprite drawn at random, by from pixels at the start down to
// to pixels at the end, over duration frames. Holds still while the game
// is frozen and leaves its last offset set.
void ScreenEffect_Shake(int32_t duration, int32_t from, int32_t to);

// Fades the play area out to color over duration frames.
void ScreenEffect_FadeOut(int32_t duration, uint32_t color);

// Fades the whole screen out to color over duration frames.
void ScreenEffect_FadeOutScreen(int32_t duration, uint32_t color);

// Flashes the play area count times, at least once, in color
// (0xAARRGGBB), fading from its alpha to 0 over duration frames each.
void ScreenEffect_Flash(int32_t duration, int32_t count, uint32_t color);
