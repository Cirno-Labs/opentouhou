#pragma once

#include "pc98/pmd/chip.h"
#include "pc98/pmd_data.h"

// Writes PMD's FM and SSG voice parameters to the chip's registers from
// instrument data.

typedef struct {
    uint8_t channel;
    uint8_t algorithm_feedback;
    uint8_t carriers;
    uint8_t levels[4];
    uint8_t key_slots;
} PMD_FM;

typedef struct {
    int16_t amount;
    uint8_t slots;
    bool enabled;
} PMD_FM_MODULATION;

// Renders PMD's full-slot FM instrument and level operations into ordered
// banked YM2608 register writes. Channel 0..5 selects the physical FM voice.
// Parameter bytes and cached levels use register order: slots 1, 3, 2, 4.
// Carrier bits use slot 4 at bit 7 through slot 1 at bit 4.
// Derives behavior from KAJA's published PMD source and the original driver:
// https://github.com/takamichih/pmdsrc
//
// These operations allocate nothing, perform no I/O and establish no timer
// cadence or synthesis. A null chip discards writes while
// retaining state changes. Channel masks, music arbitration and scheduling
// belong to the playback controller.
void PmdFm_Init(PMD_FM *voice, uint8_t channel);

// Writes TL/RR 127 for every slot, then an unconditional full-slot key-off.
// Retains the cached instrument and its original total levels.
void PmdFm_Silence(PMD_FM *voice, PMD_CHIP *chip);

// Silences all slots, writes algorithm/feedback and instrument parameters,
// and caches the four total levels. Writes instrument TL only to modulators;
// carriers receive their levels through Volume. Requires 25 parameter bytes.
void PmdFm_Program(
    PMD_FM *voice, const uint8_t parameters[PMD_INSTRUMENT_SIZE],
    PMD_CHIP *chip);

// Returns a carrier-order slot's TL, or -1 when no level write is needed.
int16_t PmdFm_Level(
    const PMD_FM *voice, uint8_t volume, const PMD_FM_MODULATION modulation[2],
    uint8_t slot);

// Writes carrier total levels with the raw PMD volume byte. Applies up to
// two supplied volume modulations in order; nullptr omits both. Each amount
// uses its signed low byte, with byte saturation before adding instrument
// TL. Volume zero omits modulation. Slots selects bits 7..4; low bits have
// no effect. Writes affected slots in order 4, 3, 2, 1. Does not change the
// cached levels or evolve modulation state. Ignores music fade/volume-down.
void PmdFm_Volume(
    const PMD_FM *voice, uint8_t volume, const PMD_FM_MODULATION modulation[2],
    PMD_CHIP *chip);

// Emits an unconditional key transition for all four slots. Rest handling
// and deciding when a transition is due belong to the playback controller.
void PmdFm_KeyOn(PMD_FM *voice, PMD_CHIP *chip);
void PmdFm_KeyOff(PMD_FM *voice, PMD_CHIP *chip);
