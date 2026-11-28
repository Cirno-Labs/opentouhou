#pragma once

#include "pc98/pmd/part.h"
#include "pc98/pmd_data.h"

// Plays one sound effect sequence over a part, independent of and able
// to interrupt the music tracks.

typedef struct {
    PMD_PART channel;
    PMD_CHIP *chip;
    PMD_CONTROL control;
    uint8_t *work;
    size_t work_size;
    uint64_t step;
    uint16_t position;
    uint16_t repeat_position;
    uint16_t instrument_offset;
    uint8_t remaining;
    uint8_t id;
    uint8_t timer_delta;
    bool playing;
    bool failed;
    bool repeat;
    bool ended;
} PMD_EFFECT_PLAYER;

// Retains mutable loop bytes across triggers. Channel selects a physical
// FM voice; upper-bank voices apply EC pan commands.
RESULT PmdEffect_Init(
    PMD_EFFECT_PLAYER *player, const PMD_EFFECTS *bank,
    const ALLOCATOR *allocator, PMD_CHIP *chip, uint8_t channel);

// Stops a current effect before replacing its channel state with defaults
// and an initial one-step countdown. Retains command mutations, shared RNG
// and the driver tie flag across triggers. Invalid IDs change nothing.
RESULT PmdEffect_Play(PMD_EFFECT_PLAYER *player, uint8_t id);

// Silences an active voice and stops its sequence. Inactive calls emit
// nothing. Retains cached channel data, command work, RNG and tie flag.
// Does not restore interrupted music parts or change timer registers.
void PmdEffect_Stop(PMD_EFFECT_PLAYER *player);

// Advances one explicit effect-engine step and emits ordered register
// writes, including pre-note key-off and terminal modulation before silence.
// timer_delta supplies the wrapping Timer A counter difference for extended
// LFOs. fade_refresh requests the driver's extra level rewrite; it does not
// attenuate effects. Neither input establishes a real timer cadence.
// On invalid data, stops with silence and sets failed; prior writes remain.
// Leaves music arbitration, synthesis and device output to the driver.
RESULT PmdEffect_Step(
    PMD_EFFECT_PLAYER *player, uint8_t timer_delta, bool fade_refresh);
