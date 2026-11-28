#pragma once

#include "th01/video.h"

// Allocates the HP bar's background cells in a caller-owned arena.
// Keep them alive across boss phases.
void HudHp_Init(const ALLOCATOR *allocator);

// Selects red, striped and white HP sections without changing snapshots.
void HudHp_Begin(uint16_t first_redwhite, uint16_t first_white);

// Empties the bar and keeps the snapshot storage.
void HudHp_Reset(void);

// Snapshots cell i at x=128+i*16, despite drawing point i at x=128+i*8.
// Snapshots use the currently selected page; drawing finishes on page 0.
void HudHp_Rerender(int16_t total);

// Restores the two-point cell containing new_hp on both pages.
// Keeps the original's wrong-section redraw for odd thresholds.
void HudHp_Decrement(int16_t new_hp);

// Snapshots and draws exactly current. Completes only at total-1==current,
// including when a changing total has already fallen below current.
bool HudHp_Increment(int16_t total, int16_t current);
