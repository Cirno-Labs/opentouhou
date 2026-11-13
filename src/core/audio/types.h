// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/base/types.h"

typedef struct AUDIO_SAMPLE AUDIO_SAMPLE;
typedef struct AUDIO_PLAYBACK AUDIO_PLAYBACK;
typedef struct AUDIO_STREAM AUDIO_STREAM;

typedef enum {
    AUDIO_LOADING,
    AUDIO_READY,
    AUDIO_FAILED,
} AUDIO_LOAD_STATE;

typedef enum {
    AUDIO_GROUP_MUSIC,
    AUDIO_GROUP_EFFECTS,
    AUDIO_GROUP_INTERFACE,
} AUDIO_GROUP;

// Counts source sample frames. One frame contains one sample per channel.
// Repeats the half-open range [begin, end) after playing the intro once.
typedef struct {
    bool enabled;
    uint64_t begin;
    uint64_t end;
} AUDIO_LOOP;

typedef struct {
    uint32_t sample_rate;
    uint32_t channels;
    bool length_known;
    uint64_t length;
} AUDIO_INFO;

// Sets linear volume, stereo balance in [-1, 1] and positive playback rate.
// Preserves source level, balance and rate with volume=1, pan=0, pitch=1.
typedef struct {
    AUDIO_GROUP group;
    float volume;
    float pan;
    float pitch;
    AUDIO_LOOP loop;
} AUDIO_PLAY_OPTIONS;
