#pragma once

#include "pc98/opna.h"

typedef struct {
    OPNA *opna;
    uint64_t next_b;
    uint8_t timer_b, timer_control, prescaler;
    uint8_t registers[16], last_noise;
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
