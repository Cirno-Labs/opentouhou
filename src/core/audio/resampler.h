#pragma once

#include "core/base/result.h"
#include "core/base/types.h"

typedef struct AUDIO_RESAMPLER AUDIO_RESAMPLER;

// Converts interleaved stereo float frames at rate to AUDIO_OUTPUT_RATE
// with a band-limited filter. Copies them when rate is the output rate.
RESULT AudioResampler_Open(uint32_t rate, AUDIO_RESAMPLER **out);
void AudioResampler_Free(AUDIO_RESAMPLER *resampler);

// Takes frames source frames that start at source frame position and
// returns the output frames that are complete, each with the source frame
// it plays. A null input flushes the filter, after which only Reset
// accepts more input. The output stays valid until the next call.
RESULT AudioResampler_Convert(
    AUDIO_RESAMPLER *resampler, const float *input, uint32_t frames,
    uint64_t position, const float **out, const uint64_t **positions,
    uint32_t *count);

// Discards the filter history, so the next input starts a new signal.
RESULT AudioResampler_Reset(AUDIO_RESAMPLER *resampler);
