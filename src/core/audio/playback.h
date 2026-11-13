#pragma once

#include "core/audio/types.h"
#include "core/base/result.h"

// Starts an independent playback of a ready sample. Reports incomplete loading.
RESULT AudioSample_Play(
    AUDIO_SAMPLE *sample, const AUDIO_PLAY_OPTIONS *options,
    AUDIO_PLAYBACK **out);

// Reports the source end or an explicit stop. Paused playback is not finished.
bool AudioPlayback_IsFinished(const AUDIO_PLAYBACK *playback);

void AudioPlayback_Restart(AUDIO_PLAYBACK *playback);
void AudioPlayback_Stop(AUDIO_PLAYBACK *playback);
void AudioPlayback_SetPaused(AUDIO_PLAYBACK *playback, bool paused);
RESULT AudioPlayback_SetVolume(AUDIO_PLAYBACK *playback, float volume);
RESULT AudioPlayback_SetPan(AUDIO_PLAYBACK *playback, float pan);
RESULT AudioPlayback_SetPitch(AUDIO_PLAYBACK *playback, float pitch);
RESULT AudioPlayback_SetLoop(AUDIO_PLAYBACK *playback, AUDIO_LOOP loop);

// Stops playback and releases the playback pointer.
void AudioPlayback_Free(AUDIO_PLAYBACK *playback);
