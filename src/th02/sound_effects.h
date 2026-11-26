#pragma once

// The sound effects of huuma.efc, named by where the game plays them; a
// sound that serves unrelated events lists the others.

// clang-format off
typedef enum {
    SOUND_EFFECT_NONE           = -1, // No effect.
    SOUND_EFFECT_SHOT           = 1,  // The player's shot.
    SOUND_EFFECT_BOSS_HEAVY_HIT = 2,  // Boss heavy hit; also miss, item drop.
    SOUND_EFFECT_PATTERN_FIRE   = 3,  // Boss pattern fires; also enemy death.
    SOUND_EFFECT_HIT            = 4,  // A boss or an enemy hit.
    SOUND_EFFECT_MIMA_BEAM      = 5,  // Mima's beam.
    SOUND_EFFECT_PILLAR         = 6,  // Pillar appears; also title flash.
    SOUND_EFFECT_PILLAR_FALL    = 7,  // Pillar's last phase; also Marisa fan.
    SOUND_EFFECT_EXTEND         = 8,  // An extra life or bomb.
    SOUND_EFFECT_ATTACK_START   = 9,  // Boss attack begins; also bomb, summon.
    SOUND_EFFECT_ATTACK_RELEASE = 10, // Boss attack fires; also dialogue.
    SOUND_EFFECT_BIT_BLOCK      = 11, // Marisa's bit blocks a hit.
    SOUND_EFFECT_ITEM           = 13, // Item pickup; also new game start.
    SOUND_EFFECT_BLAST          = 14, // Boss blast; also the vortex.
    SOUND_EFFECT_BOMB_1         = 15, // The bomb's first stage.
    SOUND_EFFECT_BOMB_2         = 16, // The bomb's second stage.
    SOUND_EFFECT_BOMB_3         = 17, // The bomb's third stage.
    SOUND_EFFECT_EXPLOSION      = 18, // A boss explosion's first frame.
} SOUND_EFFECT;
// clang-format on
