#pragma once

// The fade wait: the clock the game polls while a song fades out before it
// switches songs, counted in 48000-Hz samples and OPNA Timer A counts.

#include "core/base/types.h"

typedef struct {
    uint32_t phase;
    uint8_t timer_a;
    uint8_t volume;
    uint8_t speed;
} MUSIC_WAIT;

// Starts the wait clock at 48000 Hz with Timer A=0; Start and Stop keep its
// phase.
void MusicWait_Init(MUSIC_WAIT *wait);
void MusicWait_Start(MUSIC_WAIT *wait);
void MusicWait_Stop(MUSIC_WAIT *wait);

// Advances by frames 48000-Hz samples, fading every eighth A count; the
// fade speed clears only past 255, not at it.
void MusicWait_Advance(MUSIC_WAIT *wait, uint32_t frames);

// Starts a fade at speed 1..127.
void MusicWait_FadeOut(MUSIC_WAIT *wait, uint8_t speed);

// Reports whether attenuation reached 255.
bool MusicWait_Ready(const MUSIC_WAIT *wait);
