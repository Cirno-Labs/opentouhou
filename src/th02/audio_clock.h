#pragma once

// The audio clock: PMD's output rendered against the clock samples of each
// frame, so music and effects start where the original's driver calls did.

#include "th02/audio.h"
#include "th02/clock.h"
#include "th02/music.h"

// Binds the driver, output and fade wait until the next Init; output may
// be null.
void AudioClock_Init(PMD *pmd, GAME_AUDIO *output, MUSIC_WAIT *wait);

// Begins a tick with its measured samples, valid during the tick; without
// them the tick takes the fixed interval.
void AudioClock_Begin(const TH02_CLOCK_SAMPLE *samples, size_t count);
void AudioClock_Advance(TH02_CLOCK_KIND kind);

// Advances to the next measured effect call, using its entry sample, else
// its completion sample.
void AudioClock_Effect(PMD *pmd);
void AudioClock_End(void);

bool AudioClock_Failed(void);
RESULT AudioClock_TakeError(void);
