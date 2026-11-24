#pragma once

#include "th01/audio.h"

// Borrows loaded songs and effects through the allocator group's lifetime.
RESULT Music_Init(
    const ALLOCATOR *allocator, const MDRV_MUSIC *songs, size_t song_count,
    const MDRV_EFFECTS *effects);

// Borrows output until unbound; nullptr retains silent sample advancement.
void Music_BindOutput(GAME_AUDIO *output);

// Advances exactly 800 samples once per host tick.
void Music_Update(void);

void Music_Play(size_t song);
void Music_Fade(void);
void Music_Stop(void);
void Music_Effect(int16_t number);
void Music_Close(void);
