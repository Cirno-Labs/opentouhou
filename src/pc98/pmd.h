#pragma once

#include "pc98/pmd_data.h"
#include "pc98/opna.h"

typedef struct PMD PMD;

typedef enum { PMD_BOARD_26, PMD_BOARD_86 } PMD_BOARD;

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
