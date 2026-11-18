#pragma once

#include "core/audio/types.h"
#include "core/base/result.h"

typedef struct AUDIO_LIVE AUDIO_LIVE;

// Creates a live input at the mixer's 48000 Hz stereo rate. Requires one to
// three distinct groups and a nonzero capacity in frames. Copies the groups
// and reserves fixed buffers until Free or Audio_Shutdown. Consumes queued
// input immediately; opens no decoder or file. Uses the main-thread audio
// lifetime rules from device.h.
RESULT AudioLive_Create(
    const AUDIO_GROUP *groups, uint32_t bus_count, uint32_t capacity_frames,
    AUDIO_LIVE **out);

// Copies finite interleaved float stereo frames for every bus atomically
// under the mixer lock. Each buffer contains frames * 2 samples and may be
// reused on return. Zero frames succeeds without buffers. Successful calls
// allocate nothing and never wait for playback consumption. Invalid input
// fails before changing queued data.
//
// Keeps one shared cursor across all buses. Overflow drops the oldest frames
// from every bus together; submissions larger than capacity keep only their
// newest frames. Underrun supplies silence. Group gain and mute apply during
// mixing. Paused groups are silent while the shared timeline advances.
// Simulation may submit samples but must never query device/playback state.
RESULT AudioLive_Submit(
    AUDIO_LIVE *input, const float *const *buses, uint32_t frames);

// Drops all queued buses together without changing any producer clock.
// Accepts nullptr.
void AudioLive_Clear(AUDIO_LIVE *input);

// Unlinks the input under the mixer lock and releases its buffers. No mixer
// use remains on return. Call before Audio_Shutdown. Accepts nullptr.
void AudioLive_Free(AUDIO_LIVE *input);
