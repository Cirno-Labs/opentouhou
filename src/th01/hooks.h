#pragma once

#include "th01/session.h"

#include <stdint.h>

// Lets the replay harness watch and nudge the game: on_vsync, when set,
// runs on each reference VSync before the game handles it, on_start when a
// Start has set the session up, before gameplay begins, and on_boss when a
// boss's setup has set the boss's HP, which hp points to.
typedef struct {
    void (*on_vsync)(void *arg);
    void (*on_start)(void *arg, SESSION *session);
    void (*on_boss)(void *arg, int16_t *hp);
    void *arg;
} TH01_HOOKS;
extern TH01_HOOKS g_Th01Hooks;
