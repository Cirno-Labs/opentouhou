#pragma once

#include "pc98/mdrv_data.h"

typedef struct MDRV MDRV;

typedef struct {
    int32_t music[2];
    int32_t effects[2];
} MDRV_FRAME;

// Plays MDRV98 3.4f music and effects on one YM2608 at 48000 Hz. The FM
// balance (0..127) and rhythm balance (0..63) subtract from the driver
// volume.
RESULT Mdrv_Init(
    const MDRV_MUSIC *songs, size_t song_count, const MDRV_EFFECTS *effects,
    uint8_t fm_balance, uint8_t rhythm_balance, const ALLOCATOR *allocator,
    MDRV **out);

// Plays a zero-based song from its start and cancels the effects.
void Mdrv_Play(MDRV *mdrv, size_t song);

// Plays a one-based effect; zero does nothing.
void Mdrv_Effect(MDRV *mdrv, uint16_t effect);

// Silences the music and the effects.
void Mdrv_Stop(MDRV *mdrv);

// Fades as command F0 does: 0..127 fade out and 128..255 fade in, with the
// low seven bits as the interval in timer B IRQs, where zero means 256. A
// fade-out stops playback at full attenuation.
void Mdrv_Fade(MDRV *mdrv, uint8_t interval);

// Renders frames on the chip's own clock as music and effect sums, without
// clipping or DC removal. An SSG channel counts as effects while an effect
// holds it at the end of the frame. A null output discards the frames.
// Malformed song data is fatal.
void Mdrv_Render(MDRV *mdrv, MDRV_FRAME *output, size_t frames);
