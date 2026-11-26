#pragma once

#include "pc98/pmd/sequence.h"
#include "pc98/pmd/fm.h"
#include "pc98/pmd/ssg.h"
#include "pc98/pmd/effect.h"
#include "pc98/pmd/drums.h"
#include "pc98/pmd/rhythm.h"

#include "pc98/opna.h"

#include "pc98/pmd.h"

struct PMD {
    PMD_CHIP chip;
    PMD_BOARD board;
    MUSIC_SEQUENCE sequence;
    MUSIC_FM fm;
    MUSIC_SSG ssg;
    PMD_CONTROL control;
    PMD_EFFECT_PLAYER effect;
    SSG_EFFECTS drum;
    SSG_EFFECT_BANK drum_bank;
    RESULT error;
    PMD_RHYTHM rhythm;
    uint16_t deferred_masks;
    uint8_t timer_b, timer_a, last_timer_a, timer_delta;
    uint8_t dispatch_part;
    bool dispatching;
    bool failed, pending_a;
};

void MusicClock_StartPorts(PMD *clock);
void MusicClock_Silence(PMD *clock);

// Borrows the last physical FM voice while masked music keeps advancing.
// Restores music at stop;
// the next note boundary resumes keys. Inline masks apply after dispatch.
RESULT MusicClock_PlayEffect(PMD *clock, uint8_t id);
RESULT MusicClock_StopEffect(PMD *clock);

// Shares SSG3 with music. Melody can take over low-priority percussion.
RESULT MusicClock_PlaySsgEffect(PMD *clock, uint8_t id);
void MusicClock_StopSsgEffect(PMD *clock);
