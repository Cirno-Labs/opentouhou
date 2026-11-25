#pragma once

#include "th01/session.h"

#include <stdint.h>

// Lets the replay harness watch and nudge the game: each set hook runs on a
// reference VSync, after the session starts, or after a boss sets its HP or
// a stage its timer.
typedef struct {
    void (*on_vsync)(void *arg);
    void (*on_start)(void *arg);
    void (*on_boss)(void *arg, int16_t *hp);
    void (*on_stage_time)(void *arg, uint16_t *time);
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

// Calls on_stage_time with time, if registered.
void Th01Hooks_CallOnStageTime(uint16_t *time);
