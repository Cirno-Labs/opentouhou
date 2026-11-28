#pragma once

#include "pc98/pmd/part.h"

// Plays PMD86's built-in percussion samples through the YM2608's rhythm
// unit.

typedef struct {
    PMD_CHIP *chip;
    PMD_CONTROL *control;
    uint8_t levels[6];
    uint8_t volume;
    uint8_t volume_down;
} PMD_RHYTHM;

// Routes PMD86 percussion through one YM2608. Init borrows chip and control;
// Reset selects centered voices and the default total level.
void PmdRhythm_Init(PMD_RHYTHM *rhythm, PMD_CHIP *chip, PMD_CONTROL *control);
void PmdRhythm_Reset(PMD_RHYTHM *rhythm);

// Applies key, level, pan and relative-level commands E5/E6/E8..EB.
// Returns failure for invalid voice selectors or commands.
RESULT PmdRhythm_Command(
    PMD_RHYTHM *rhythm, uint8_t opcode, const uint8_t *parameters);

// Triggers every selected K-part instrument, including closed-hi-hat dump.
// Applies the shared fade before SSG percussion arbitration.
void PmdRhythm_Play(PMD_RHYTHM *rhythm, uint16_t mask);
