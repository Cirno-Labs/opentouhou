#pragma once

#include "core/base/types.h"

#include <stddef.h>

// The sound effects of ZIGOKU.MDE, named by where the game plays them; a
// sound that serves unrelated events lists the others.
// clang-format off
typedef enum {
    SOUND_EFFECT_SHOT       = 1,  // The player's shot.
    SOUND_EFFECT_HIT        = 2,  // A card flips; also the bonus digit tick.
    SOUND_EFFECT_BOSS_HIT   = 3,  // The orb hits a boss; also b20m debris.
    SOUND_EFFECT_CONFIRM    = 4,  // A route choice confirmed.
    SOUND_EFFECT_MISS       = 5,  // A life lost; also b05's final defeat.
    SOUND_EFFECT_LASER      = 6,  // A boss laser; also missiles, hazards.
    SOUND_EFFECT_PELLET     = 7,  // A pellet volley; also missiles, shakes.
    SOUND_EFFECT_TELEGRAPH  = 8,  // A boss attack begins; also the bomb.
    SOUND_EFFECT_SHAKE      = 9,  // A screen shake's tick; also the bomb.
    SOUND_EFFECT_GUST       = 10, // b20m's gust or star petals.
    SOUND_EFFECT_STRIKE     = 11, // The player's strike, slide or deflect.
    SOUND_EFFECT_SPAWN      = 12, // b20j's snake; also b10j's pillars.
    SOUND_EFFECT_TITLE_TEXT = 14, // A title screen character drawn.
    SOUND_EFFECT_ITEM       = 15, // An item picked up; also an extra life.
    SOUND_EFFECT_SHOT_HIT   = 16, // A shot deflects a pellet or hits a boss.
    SOUND_EFFECT_TIME_UP    = 17, // The stage timer runs out.
} SOUND_EFFECT;
// clang-format on

// Ignores effects, playback, fades and stops while gameplay music is off.
void Sound_Effect(SOUND_EFFECT number);

// Selects the song that Sound_Play plays.
void Sound_Load(size_t song);

void Sound_Play(void);
void Sound_Fade(void);
void Sound_Stop(void);
