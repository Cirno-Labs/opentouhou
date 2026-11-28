#pragma once

#include "th01/hud/hud.h"

#define HUD_RESOURCE_CAPACITY 32

typedef struct {
    uint8_t *lives;
    uint8_t *bombs;
    uint8_t *numbers;
    uint8_t *stage;
    uint8_t *timer;
    uint32_t score_previous;
    uint32_t high_previous;
    int16_t combo_previous;
    int16_t max_previous;
    uint8_t maximum_combo;
    bool force_numbers;
    const GRF *mask;
    const PTN *sprites;
    const GAME_TEXT_FONT *font;
} HUD;

// Writable view of the HUD's state for tests.
extern HUD *const p_Hud;
