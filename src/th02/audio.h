#pragma once

// The game's audio output: one stream in which the PMD music and the sound
// effects share the music group, as they share one FM chip in the original.

#include "pc98/pmd.h"

typedef struct GAME_AUDIO GAME_AUDIO;

// Creates the game's audio output, sharing AUDIO_GROUP_MUSIC between music
// and effects; requires initialized core audio.
RESULT GameAudio_Prepare(const ALLOCATOR *allocator, GAME_AUDIO **out);

// Renders frames of music and effects; a null output advances them
// silently.
RESULT GameAudio_Render(GAME_AUDIO *output, PMD *pmd, size_t frames);

// Frees the output before Audio_Shutdown and its allocator; accepts null.
void GameAudio_Free(GAME_AUDIO *output);
