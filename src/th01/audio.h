#pragma once

#include "pc98/mdrv_synth.h"

typedef struct GAME_AUDIO GAME_AUDIO;

// Prepares one synchronized Music/Effects input for the shared producer.
// Requires initialized core audio. Reserves conversion work in allocator's
// group; keep that group alive until Free, before Audio_Shutdown. A failed
// preparation leaves out null and no live input. Queues at most 3200 frames;
// core applies synchronized overflow drops and silent underruns.
RESULT GameAudio_Prepare(const ALLOCATOR *allocator, GAME_AUDIO **out);

// Advances the producer and submits its component buses without allocation
// or device queries. A null output advances and discards the same samples.
// Converts with fixed 1/65536 gain and a 20 Hz DC blocker per bus/channel:
// y[n] = x[n] - x[n-1] + 0.99738543*y[n-1]. Keeps filter history across
// blocks, music controls and world changes. Does not clip or normalize.
RESULT GameAudio_Render(GAME_AUDIO *output, MDRV_SYNTH *synth, size_t frames);

// Releases the live input, retaining allocator-owned conversion storage.
// Accepts null. Call once before discarding its allocation group.
void GameAudio_Free(GAME_AUDIO *output);
