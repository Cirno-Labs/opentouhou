#pragma once

#include "pc98/pmd/part.h"

typedef struct {
    PMD_PART channels[3];
    PMD_CONTROL *control;
    PMD_CHIP *chip;
    uint8_t noise, volume_down, effect_id;
    bool effect_active;
} MUSIC_SSG;

// Shares driver control and the chip register shadow with FM and percussion.
void MusicSsg_Init(MUSIC_SSG *ssg, PMD_CONTROL *control, PMD_CHIP *chip);

// Reset and stop retain DAC levels and leave SSG3 to an active effect.
// Noise writes wait until effect_id bit 7 is set, as after termination.
void MusicSsg_Reset(MUSIC_SSG *ssg);
void MusicSsg_Stop(MUSIC_SSG *ssg);

void MusicSsg_Begin(
    MUSIC_SSG *ssg, uint8_t part, uint8_t mask, uint8_t remaining);
void MusicSsg_Advance(
    MUSIC_SSG *ssg, uint8_t part, uint8_t mask, uint8_t timer_delta);
RESULT MusicSsg_Note(
    MUSIC_SSG *ssg, uint8_t part, uint8_t mask, uint8_t note, uint8_t duration,
    bool slur, bool next_tie, uint8_t timer_delta);
RESULT MusicSsg_Glide(
    MUSIC_SSG *ssg, uint8_t part, uint8_t mask, uint8_t note, uint8_t target,
    uint8_t duration, bool slur, bool next_tie, uint8_t timer_delta);
void MusicSsg_End(MUSIC_SSG *ssg, uint8_t part);
void MusicSsg_Command(
    MUSIC_SSG *ssg, uint8_t part, uint8_t mask, uint8_t opcode,
    const uint8_t *parameters);
