// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/patch/stack.h"

// Shows translator notes over the screen, as thcrap shows them with
// nmlgc/base_tsa in the patch stack. A note is the text after U+0014 in a
// text that th06 draws. The "tlnotes" object of the patch settings sets
// where and how notes show:
// - region_topleft and region_size give the region, in pixels of
//   reference_resolution, which spans the whole screen;
// - font is a thcrap font rule, "'Face' height width weight quality". The
//   patch font of the "font" setting replaces its face, as thcrap's font
//   hooks do;
// - outline_radius, fade_ms, read_speed (UTF-8 bytes a second) and valign
//   default to 2, 500, 35 and "bottom".
// A note is drawn at the reference resolution and scaled to the screen.
// Its lines break between words to fit the width of the region, and each
// line is centered. The text is white with a black outline. The note is
// centered in the region and aligned by valign. It fades in over fade_ms
// and stays until Note_Remove. A note taller than the region scrolls
// through it in the time that reading its bytes takes, then starts again.
// thcrap counts this time in milliseconds of the clock; th06 counts 1000/60
// milliseconds a tick.
//
// Without nmlgc/base_tsa, or without a region in the settings, notes do
// not show and the screen stays as it is. Notes change only what is drawn.

// Reads the settings and opens the font, when nmlgc/base_tsa is in the
// patch stack. Call after Translation_Init and Anm_InitText. A setting
// that is not valid keeps its default, with a log line.
void Note_Init(const PATCH_STACK *patches);
// Releases the notes and the font. Call before Anm_ShutdownText.
void Note_Shutdown(void);

// Shows the translator note of text: from the first U+0014 to the next
// one or to the end. A text without a note, or with an empty one, changes
// nothing.
void Note_Show(const char *text);
// Hides the shown note.
void Note_Remove(void);
// Draws the shown note over the screen, as thcrap does when the original
// ends its scene: once a tick, after the draw chain. A note starts to
// fade in on the first call that finds it shown in place of another note
// or of none.
void Note_Draw(void);
