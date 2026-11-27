#pragma once

#include "pc98/opna.h"
#include "pc98/pmd/drums.h"
#include "pc98/pmd/effect.h"
#include "pc98/pmd/fm.h"
#include "pc98/pmd/rhythm.h"
#include "pc98/pmd/sequence.h"
#include "pc98/pmd/ssg.h"
#include "pc98/pmd/types.h"
#include "pc98/pmd_data.h"

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
    uint8_t timer_b;
    uint8_t timer_a;
    uint8_t last_timer_a;
    uint8_t timer_delta;
    uint8_t dispatch_part;
    bool dispatching;
    bool failed;
    bool pending_a;
};

// Plays PMD 4.8 songs and effects at 48000 Hz. Board 26 has three FM
// voices and SSG drums; board 86 has six FM voices and hardware rhythm.
// Effects borrow the last physical FM voice.
// Loads the first Timer B interval from initial_timer_b, then selects
// the idle value 200 without reloading that interval.
// Malformed playback data is fatal.
RESULT Pmd_Init(
    const PMD_EFFECTS *effects, PMD_BOARD board, uint8_t initial_timer_b,
    const ALLOCATOR *allocator, PMD **out);

// Replaces song data without stopping playback. Stop first when changing
// songs outside an original driver sequence that loads while playing.
void Pmd_Load(PMD *pmd, const PMD_SONG *song);
void Pmd_Start(PMD *pmd);
void Pmd_Stop(PMD *pmd);
void Pmd_Fade(PMD *pmd, int8_t speed);
void Pmd_Effect(PMD *pmd, uint8_t effect);

// Retains the cumulative measure count after stopping playback.
uint16_t Pmd_Measure(const PMD *pmd);

void Pmd_Render(PMD *pmd, OPNA_FRAME *output, size_t frames);

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
