#pragma once

#include "th01/video.h"

#define HUD_HP_CAPACITY 32

typedef struct {
    uint8_t *backgrounds;
    uint16_t first_redwhite, first_white;
} HUD_HP;

extern HUD_HP g_HudHp;

// Allocates raw 16x16 background cells from a caller-owned arena. Keep them
// alive across boss phases. Thresholds are borrowed source values copied by
// the boss before each operation; they may be odd. Normal bosses use 8..18
// points. Defines a safe domain of 0..32 points instead of the original
// debug overflow: invalid totals or point indices leave all state unchanged.
// Drawing clips to the visible page and changes only the drawing selector.
// Operations allocate nothing and read no fonts, files, clocks or devices.
void HudHp_Init(const ALLOCATOR *allocator);

// Empties the bar and keeps the snapshot storage.
void HudHp_Reset(void);

// Snapshots cell i at x=128+i*16, despite drawing point i at x=128+i*8.
// Snapshots use the currently selected page; drawing finishes on page 0.
void HudHp_Rerender(int16_t total);

// Restores the two-point cell containing new_hp on page 1 then page 0.
// Odd new_hp redraws new_hp-1 using the section chosen by new_hp, preserving
// the wrong-section pattern when the thresholds are odd.
void HudHp_Decrement(int16_t new_hp);

// Snapshots and draws exactly current. Completes only at total-1==current,
// including when a changing total has already fallen below current.
bool HudHp_Increment(int16_t total, int16_t current);
