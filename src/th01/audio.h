#pragma once

#include "pc98/mdrv.h"

typedef struct GAME_AUDIO GAME_AUDIO;

// Prepares one synchronized Music/Effects input, queuing at most 3200 frames.
// Keep allocator's group alive until Free, before Audio_Shutdown.
RESULT GameAudio_Prepare(const ALLOCATOR *allocator, GAME_AUDIO **out);

// Advances the producer and submits its buses; a null output discards them.
// Applies a 20 Hz DC blocker per bus and channel, without clipping.
void GameAudio_Render(GAME_AUDIO *output, MDRV *mdrv, size_t frames);

// Releases the live input, retaining allocator-owned conversion storage.
// Accepts null. Call once before discarding its allocation group.
void GameAudio_Free(GAME_AUDIO *output);
