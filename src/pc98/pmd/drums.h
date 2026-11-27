#pragma once

#include "core/base/result.h"
#include "pc98/pmd/chip.h"

#define SSG_EFFECT_LIMIT 128

typedef struct {
    const uint8_t *bytes;
    size_t size;
    uint16_t offsets[SSG_EFFECT_LIMIT];
    uint8_t priorities[SSG_EFFECT_LIMIT];
    uint8_t count;
} SSG_EFFECT_BANK;

typedef struct {
    const SSG_EFFECT_BANK *bank;
    PMD_CHIP *chip;
    uint16_t position;
    uint16_t tone;
    int16_t tone_sweep;
    uint8_t noise;
    uint8_t noise_sweep;
    uint8_t noise_count;
    uint8_t count;
    uint8_t priority;
    uint8_t id;
} SSG_EFFECTS;

// Binds the static table compiled into the original PMD driver.
RESULT SsgEffects_LoadTable(SSG_EFFECT_BANK *out);

// Shares the borrowed chip and its register shadow with music playback.
void SsgEffects_Init(
    SSG_EFFECTS *effects, const SSG_EFFECT_BANK *bank, PMD_CHIP *chip);

// Sets the requested ID and replaces an equal or lower priority effect.
// Rejected lower-priority requests still update ID, as in PMD. Accepted
// requests emit their first frame immediately. Does not arbitrate music
// masks or emulate PPSDRV corrections. Invalid IDs change nothing.
RESULT SsgEffects_Play(SSG_EFFECTS *effects, uint8_t id);

// Advances one explicit Timer A call: countdown, next frame or tone/noise
// sweeps. Counts and periods wrap as bytes/words. Success allocates nothing
// and performs no I/O. Inactive calls do nothing.
RESULT SsgEffects_Step(SSG_EFFECTS *effects);

// Always writes volume zero and disables SSG3 tone/noise, then clears
// priority and sets ID to FF. Retains countdown and sweep work.
void SsgEffects_Stop(SSG_EFFECTS *effects);
