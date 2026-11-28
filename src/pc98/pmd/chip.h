#pragma once

#include "pc98/opna.h"

// Shares one YM2608 and its timer B between PMD's FM, SSG, rhythm and
// effect parts.

typedef struct {
    OPNA *opna;
    uint64_t next_b;
    uint8_t timer_b;
    uint8_t timer_control;
    uint8_t prescaler;
    uint8_t registers[16];
    uint8_t last_noise;
} PMD_CHIP;

void PmdChip_Init(
    PMD_CHIP *chip, const ALLOCATOR *allocator, OPNA_TIMER_CALLBACK callback,
    void *context);

// Retains loaded timer intervals across register and prescaler changes.
// A null chip discards writes for muted FM instrument updates.
void PmdChip_Write(PMD_CHIP *chip, uint16_t reg, uint8_t value);
void PmdChip_ReloadTimerB(PMD_CHIP *chip, uint64_t time);

// Updates the shared noise cache before writing percussion registers.
void PmdChip_PercussionWrite(PMD_CHIP *chip, uint8_t reg, uint8_t value);
