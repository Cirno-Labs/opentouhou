#pragma once

#include "pc98/mdrv_seq.h"
#include "pc98/opna.h"

typedef struct MDRV_SYNTH MDRV_SYNTH;

typedef struct {
    uint8_t fm_balance, rhythm_balance;
} MDRV_SYNTH_CONFIG;

typedef struct {
    int32_t music[2], effects[2];
} MDRV_SYNTH_FRAME;

typedef struct {
    uint64_t time, pulses;
    uint8_t sync, timer_b, fade_attenuation, effect_mask;
    bool music_playing, effects_playing, fading, failed;
} MDRV_SYNTH_STATUS;

typedef void (*MDRV_REGISTER_CALLBACK)(
    void *context, uint64_t time, uint16_t reg, uint8_t value);

// Produces deterministic 48000 Hz PCM from immutable MDRV assets and one
// shared OPNA. Derives driver controls from the original MDRV98.COM 3.4f
// binary and the author's MDRV2 documentation. Models one YM2608 board.
// Uses no original executable code in playback. See opna.h for chip-model
// provenance and limitations. Copies config at initialization; FM balance
// (0..127) and rhythm balance (0..63) subtract from driver volume before
// register conversion. Zero applies no balance offset. Reset, Stop and Play
// retain these settings.
//
// Init reads instrument fields through READER and reserves all playback work
// from allocator. Keep the assets and allocator's lifetime group alive until
// the producer is discarded. Discard its allocation group if Init fails.
// Reset, successful controls and Render allocate nothing. RESULT failure
// messages can allocate on the core heap. Load every song before Init; Play
// selects a zero-based song index. Effects use DOS's one-based numbers; zero
// does nothing. Stop silences both music and effects. Play cancels effects and
// clears channel work, retaining the source driver's global timer reload.
// Header repeat retains volume and program controls. RestartFadeIn follows API
// 0F: restarts the selected song with retained volume/program controls and a
// fade-in interval of two B IRQs. Reset rebuilds chip and driver state in
// place, including the deterministic timer phase.
//
// Render advances its own chip timeline, independent of host clocks, devices
// and game ticks. Timer B status drives ordered sequencer clocks, followed by
// fade work. Fade uses F0's encoding: 0..127 fade out, 128..255 fade in, with
// the low seven bits as an interval in B IRQs; zero means 256 IRQs. A fade-out
// stops both timelines at attenuation 127. A blocking game fade must advance
// this simulation timeline, never poll an audio device.
//
// Returns signed stereo music and effect component sums without clipping,
// gain or DC removal. SSG DAC contributions are unipolar, as in opna.h.
// Routes each SSG frame to effects while its physical effect pointer is
// active at the end of the interval, otherwise to music. Unclaimed SSG output
// joins music, including register values retained after an effect. This rule
// includes ownership changes within the interval. All components share the
// chip's noise and envelope generators. Final mixing and the audio device
// belong to a separate integration. A nullptr output discards samples.
//
// Calls the optional register observer synchronously for actual chip writes,
// at the chip's exact integer Time. IRQ writes share the overflow timestamp;
// CPU interrupt and port delays are not modeled. Do not reenter operations.
// Rejects external PCM tracks and SSG external PCM programs, which TH01 does
// not use. Malformed commands or program references latch failure and silence
// the chip; Render fills the remaining buffer with zeros and returns the
// error. Play or Reset clears failure. No files or platform APIs enter clocks.
RESULT MdrvSynth_Init(
    const MDRV_MUSIC *songs, size_t song_count, const MDRV_EFFECTS *effects,
    MDRV_SYNTH_CONFIG config, const ALLOCATOR *allocator,
    MDRV_REGISTER_CALLBACK callback, void *context, MDRV_SYNTH **out);
void MdrvSynth_Reset(MDRV_SYNTH *synth);
RESULT MdrvSynth_Play(MDRV_SYNTH *synth, size_t song);
RESULT MdrvSynth_RestartFadeIn(MDRV_SYNTH *synth);
RESULT MdrvSynth_EffectPlay(MDRV_SYNTH *synth, uint16_t effect);
void MdrvSynth_Stop(MDRV_SYNTH *synth);
void MdrvSynth_Fade(MDRV_SYNTH *synth, uint8_t interval);
RESULT MdrvSynth_Render(
    MDRV_SYNTH *synth, MDRV_SYNTH_FRAME *output, size_t frames);
MDRV_SYNTH_STATUS MdrvSynth_Status(const MDRV_SYNTH *synth);
