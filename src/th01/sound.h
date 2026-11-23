#pragma once

#include "pc98/mdrv.h"

#include <stddef.h>

// Holds REIIDEN's music driver and the song that was last loaded into it.
// Effect, Play, Fade and Stop do nothing while the music setting is off.
typedef struct {
    MDRV *mdrv;
    size_t song;
} SOUND;

extern SOUND g_Sound;

void Sound_Effect(int16_t number);

// Selects the song that Sound_Play plays.
void Sound_Load(size_t song);

void Sound_Play(void);
void Sound_Fade(void);
void Sound_Stop(void);
