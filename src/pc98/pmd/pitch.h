#pragma once

#include "pc98/pmd/registers.h"

typedef struct {
    uint16_t frequency, glide;
    int16_t glide_step, glide_remainder;
    uint8_t note;
} PMD_PITCH;

// Reproduces PMD's byte transpose, ordinary FM note frequencies and
// full-slot pitch register operations. Derives behavior from KAJA's
// published PMD source and the original driver:
// https://github.com/takamichih/pmdsrc
// Keeps chip output, modulation evolution and controller timing separate.
void PmdPitch_Init(PMD_PITCH *pitch);

// Adds transpose and master as bytes, then applies their signed sum.
// Preserves byte octave rotation and wrap; only exact note 0F bypasses
// transposition. Does not resolve the controller's previous-note marker.
uint8_t PmdPitch_Transpose(uint8_t note, int8_t transpose, int8_t master);

// Sets frequency from note's octave bits 6..4 and semitone bits 3..0.
// A low nibble F sets note to FF and keeps frequency only when requested.
// Rejects semitones C..E without changing state. Retains glide fields;
// clearing glide on a new pitched note belongs to the controller.
RESULT PmdPitch_Note(PMD_PITCH *pitch, uint8_t note, bool keep_rest_frequency);

// Sets a glide's signed quotient and remainder toward target_frequency
// over duration steps. Uses encoded block/F-number values; retains the
// base note, frequency and current glide offset. Rejects zero duration
// without changing state. Does not decide when a glide starts or advances.
RESULT PmdPitch_StartGlide(
    PMD_PITCH *pitch, uint16_t target_frequency, uint8_t duration);

// Adds the quotient with word wrap, then moves a nonzero remainder one
// unit toward zero and adds that signed unit to the offset.
void PmdPitch_GlideStep(PMD_PITCH *pitch);

uint16_t PmdPitch_Frequency(
    const PMD_PITCH *pitch, int16_t detune,
    const PMD_FM_MODULATION modulation[2]);

// Emits high then low pitch registers for physical FM channel 0..5.
// Adds glide, detune and up to two enabled modulation words with 16-bit
// wrap before PMD's block normalization and end clamps. Modulation slots
// are ignored for ordinary full-slot pitch; nullptr omits modulation.
// A zero base frequency emits nothing. Writes to the owned chip.
// Allocates nothing and establishes no timer cadence or synthesis.
void PmdPitch_Render(
    const PMD_PITCH *pitch, uint8_t channel, int16_t detune,
    const PMD_FM_MODULATION modulation[2], PMD_CHIP *chip);
