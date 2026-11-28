#pragma once

// The audio clock: PMD's output rendered against the clock samples of each
// frame, so music and effects start where the original's driver calls did.

#include "th02/audio.h"
#include "th02/clock.h"
#include "th02/music/wait.h"

// The clock's render call, set by AudioClock_InitRender: the real
// GameAudio_Render by default, a fake observer in tests.
typedef struct {
    RESULT (*render)(GAME_AUDIO *output, PMD *pmd, size_t frames);
} AUDIO_CLOCK_RENDER;

extern const AUDIO_CLOCK_RENDER g_AudioClockRender;

// Binds the driver, output and fade wait until the next Init; output may
// be null.
void AudioClock_Init(PMD *pmd, GAME_AUDIO *output, MUSIC_WAIT *wait);

// Swaps the clock's render call; defaults to g_AudioClockRender, so only
// tests need to call this.
void AudioClock_InitRender(const AUDIO_CLOCK_RENDER *render);

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
