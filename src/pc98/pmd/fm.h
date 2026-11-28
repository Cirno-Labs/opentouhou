#pragma once

#include "pc98/pmd/part.h"
#include "pc98/pmd/types.h"
#include "pc98/pmd_data.h"

// Drives PMD's six FM music channels, one part each over the shared
// chip.

typedef struct {
    PMD_PART channels[6];
    PMD_CHIP *chip;
    PMD_CONTROL *control;
    uint8_t volume_down;
    uint8_t pans[6];
    uint8_t channel_count;
    PMD_BOARD board;
    bool effect_active;
} MUSIC_FM;

void MusicFm_Init(
    MUSIC_FM *fm, PMD_CONTROL *control, PMD_CHIP *chip, PMD_BOARD board);

// Reset and stop leave the last FM voice to an active effect.
// Reset retains shared RNG.
void MusicFm_Reset(MUSIC_FM *fm);
void MusicFm_Stop(MUSIC_FM *fm);

void MusicFm_Begin(MUSIC_FM *fm, uint8_t part, uint8_t mask, uint8_t remaining);
void MusicFm_Advance(
    MUSIC_FM *fm, uint8_t part, uint8_t mask, uint8_t timer_delta);
RESULT MusicFm_Note(
    MUSIC_FM *fm, uint8_t part, uint8_t mask, uint8_t note, uint8_t duration,
    bool slur, bool next_tie, uint8_t timer_delta);
RESULT MusicFm_Glide(
    MUSIC_FM *fm, uint8_t part, uint8_t mask, uint8_t note, uint8_t target,
    uint8_t duration, bool slur, bool next_tie, uint8_t timer_delta);
void MusicFm_End(MUSIC_FM *fm, uint8_t part);

// FM3 slot voices remain silent stubs. Commands retain prior writes on error.
RESULT MusicFm_Command(
    MUSIC_FM *fm, uint8_t part, uint8_t mask, const PMD_SONG *song,
    uint8_t opcode, const uint8_t *parameters);

// Restores the last physical FM voice without rekeying. Program zero can match
// the bank terminator and read its parameters from retained resident work after
// the song bytes.
RESULT MusicFm_Restore(
    MUSIC_FM *fm, const PMD_SONG *song, const uint8_t *work, size_t work_size);
