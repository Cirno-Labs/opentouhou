#pragma once

#include "th01/session.h"

#include <stdint.h>

// Lets the replay harness watch and nudge the game: on_vsync, when set,
// runs on each reference VSync before the game handles it, on_start when a
// Start has set the session up, before gameplay begins, and on_boss when a
// boss's setup has set the boss's HP, which hp points to.
typedef struct {
    void (*on_vsync)(void *arg);
    void (*on_start)(void *arg);
    void (*on_boss)(void *arg, int16_t *hp);
    void *arg;
} TH01_HOOKS;

// Replaces the registered hooks wholesale, as the replay harness sets up.
void Th01Hooks_Set(TH01_HOOKS hooks);

// Calls on_start, if registered.
void Th01Hooks_CallOnStart(void);

// Calls on_vsync, if registered.
void Th01Hooks_CallOnVSync(void);

// Calls on_boss with hp, if registered.
void Th01Hooks_CallOnBoss(int16_t *hp);
