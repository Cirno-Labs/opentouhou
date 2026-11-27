#pragma once

#include "th02/session.h"

// Replay hooks: on_vsync runs before each reference VSync, and on_start
// sets the start values after shot selection, before MAIN or MAINE.
typedef struct {
    void (*on_vsync)(void *arg);
    void (*on_start)(void *arg, TH02_SESSION *session);
    // Runs on each boss damage reset; threshold is the smallest damage that
    // defeats the boss or advances its phase.
    void (*on_boss)(void *arg, int16_t *damage, int16_t threshold);
    void *arg;
} TH02_HOOKS;

extern TH02_HOOKS g_Th02Hooks;
