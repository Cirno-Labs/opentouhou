#pragma once

#include "pc98/pmd/lfo.h"
#include "pc98/pmd/pitch.h"

// Tracks one note's gate, pitch and LFO state; FM, SSG and effect
// channels each hold one part.

typedef struct {
    uint8_t previous;
    uint8_t gate;
    uint8_t key_flags;
} PMD_NOTE;

typedef struct {
    uint8_t fixed;
    uint8_t proportional;
    uint8_t minimum;
} PMD_GATE;

typedef struct {
    uint8_t volume;
    uint8_t speed;
    bool stop_on_end;
    bool pending_stop;
    bool inline_requested;
} PMD_FADE;

typedef struct {
    uint16_t random_seed;
    uint8_t volume_push;
    bool tied;
    PMD_FADE fade;
} PMD_CONTROL;

typedef struct {
    uint8_t mode;
    uint8_t stage;
    uint8_t level;
    // Shares rate bytes with the normal envelope; counters 0/2/3 retain
    // their initial attack, decay and release rates.
    uint8_t rates[4];
    uint8_t counters[4];
    uint8_t sustain;
    uint8_t start_level;
} PMD_PART_ENVELOPE;

typedef struct {
    PMD_FM fm;
    PMD_PITCH pitch;
    PMD_LFO lfos[2];
    PMD_NOTE note;
    PMD_GATE gate;
    PMD_PART_ENVELOPE envelope;
    int16_t detune;
    int8_t transpose;
    int8_t master;
    uint8_t lfo_flags;
    uint8_t lfo_modes;
    uint8_t volume;
    uint8_t program;
    uint8_t volume_masks[2];
    uint8_t one_volume;
    uint8_t pattern;
    bool new_note;
    bool partial_slots;
} PMD_PART;

RESULT PmdPart_Note(
    PMD_PART *part, PMD_CONTROL *control, bool ssg, uint8_t raw_note,
    uint8_t duration, bool slur, bool glide, uint8_t target,
    uint8_t timer_delta);

uint8_t PmdPart_Advance(
    PMD_PART *part, uint16_t *random_seed, uint8_t timer_delta);

bool PmdPart_Command(
    PMD_PART *part, PMD_CONTROL *control, bool ssg, uint8_t opcode,
    const uint8_t *parameters);

bool PmdPart_KeyOffDue(PMD_PART *part, uint8_t remaining);

void PmdPart_Arm(PMD_PART *part, bool next_tie);
