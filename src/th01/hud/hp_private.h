#pragma once

#include "th01/hud/hp.h"

#define HUD_HP_CAPACITY 32

typedef struct {
    uint8_t *backgrounds;
    uint16_t first_redwhite;
    uint16_t first_white;
} HUD_HP;

// Writable view of the HP bar's state for tests.
extern HUD_HP *const p_Health;
