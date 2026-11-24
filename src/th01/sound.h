#pragma once

#include "core/base/types.h"

#include <stddef.h>

// Ignores effects, playback, fades and stops while gameplay music is off.
void Sound_Effect(int16_t number);

// Selects the song that Sound_Play plays.
void Sound_Load(size_t song);

void Sound_Play(void);
void Sound_Fade(void);
void Sound_Stop(void);
