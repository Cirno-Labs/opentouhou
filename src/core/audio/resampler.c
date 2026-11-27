#include "core/audio/resampler.h"

#include "core/audio/backend.h"
#include "core/base/memory.h"
#include "core/base/utils.h"

#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libswresample/swresample.h>
#include <string.h>

#define RESAMPLER_CHANNELS 2
#define RESAMPLER_PIECE_FRAMES 1024
// Holds the source positions of the last input frames. It covers one
// piece and the filter delay.
#define RESAMPLER_HISTORY_FRAMES 4096
#define RESAMPLER_PAD_FRAMES 64
#define RESAMPLER_PAD_LIMIT 64

struct AUDIO_RESAMPLER {
    // Null when the source rate is the output rate.
    SwrContext *context;
    uint32_t rate;
    uint64_t input_frames;
    uint64_t output_frames;
    uint64_t history[RESAMPLER_HISTORY_FRAMES];
    float *buffer;
    uint64_t *positions;
    uint32_t capacity;
};

static const float m_Silence[RESAMPLER_PAD_FRAMES * RESAMPLER_CHANNELS];

static RESULT M_Error(const char *const operation, const int error)
{
    char message[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(error, message, sizeof(message));
    return FAIL("%s: %s", operation, message);
}

static void M_Reserve(AUDIO_RESAMPLER *const resampler, const uint32_t frames)
{
    if (frames > resampler->capacity) {
        const uint32_t capacity = MAX(frames, resampler->capacity * 2);
        resampler->buffer = Mem_ReallocArr(
            resampler->buffer, capacity, RESAMPLER_CHANNELS * sizeof(float));
        resampler->positions =
            Mem_ReallocArr(resampler->positions, capacity, sizeof(uint64_t));
        resampler->capacity = capacity;
    }
}

// Converts frames input frames and appends the output after total frames.
static RESULT M_Run(
    AUDIO_RESAMPLER *const resampler, const float *const input,
    const uint32_t frames, uint32_t *const total)
{
    uint32_t count = frames;
    if (resampler->context == nullptr) {
        M_Reserve(resampler, *total + frames);
        memcpy(
            resampler->buffer + (size_t)*total * RESAMPLER_CHANNELS, input,
            (size_t)frames * RESAMPLER_CHANNELS * sizeof(float));
    } else {
        const int capacity =
            swr_get_out_samples(resampler->context, (int)frames);
        if (capacity < 0) {
            return M_Error("cannot size audio resampling", capacity);
        }
        M_Reserve(resampler, *total + (uint32_t)capacity);
        uint8_t *output = (uint8_t *)(resampler->buffer
                                      + (size_t)*total * RESAMPLER_CHANNELS);
        const uint8_t *source = (const uint8_t *)input;
        const int converted = swr_convert(
            resampler->context, &output, capacity, &source, (int)frames);
        if (converted < 0) {
            return M_Error("cannot resample audio", converted);
        }
        count = (uint32_t)converted;
    }
    resampler->input_frames += frames;
    for (uint32_t i = 0; i < count; i++) {
        // Output frame n plays at source time n * rate / AUDIO_OUTPUT_RATE.
        uint64_t at =
            resampler->output_frames * resampler->rate / AUDIO_OUTPUT_RATE;
        at = MIN(at, resampler->input_frames - 1);
        resampler->positions[*total + i] =
            resampler->history[at % RESAMPLER_HISTORY_FRAMES];
        resampler->output_frames++;
    }
    *total += count;
    return OK;
}

// Pushes the last input frames through the filter with silence. A plain
// flush of swresample returns nothing for an input shorter than its filter.
static RESULT M_Flush(AUDIO_RESAMPLER *const resampler, uint32_t *const total)
{
    const uint64_t end =
        (resampler->input_frames * AUDIO_OUTPUT_RATE + resampler->rate - 1)
        / resampler->rate;
    for (int i = 0; i < RESAMPLER_PAD_LIMIT && resampler->output_frames < end;
         i++) {
        MUST(M_Run(resampler, m_Silence, RESAMPLER_PAD_FRAMES, total));
    }
    if (resampler->output_frames > end) {
        *total -= (uint32_t)(resampler->output_frames - end);
        resampler->output_frames = end;
    }
    return OK;
}

RESULT AudioResampler_Open(const uint32_t rate, AUDIO_RESAMPLER **const out)
{
    *out = nullptr;
    FAIL_IF(
        rate == 0 || rate > INT32_MAX, "invalid audio sample rate %u", rate);
    AUDIO_RESAMPLER *const resampler = Mem_AllocZ(sizeof(*resampler));
    resampler->rate = rate;
    if (rate != AUDIO_OUTPUT_RATE) {
        AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
        int error = swr_alloc_set_opts2(
            &resampler->context, &stereo, AV_SAMPLE_FMT_FLT, AUDIO_OUTPUT_RATE,
            &stereo, AV_SAMPLE_FMT_FLT, (int)rate, 0, nullptr);
        if (error >= 0) {
            error = swr_init(resampler->context);
        }
        if (error < 0) {
            AudioResampler_Free(resampler);
            return M_Error("cannot start audio resampling", error);
        }
    }
    *out = resampler;
    return OK;
}

void AudioResampler_Free(AUDIO_RESAMPLER *const resampler)
{
    if (resampler != nullptr) {
        swr_free(&resampler->context);
        Mem_Free(resampler->buffer);
        Mem_Free(resampler->positions);
        Mem_Free(resampler);
    }
}

RESULT AudioResampler_Convert(
    AUDIO_RESAMPLER *const resampler, const float *const input,
    const uint32_t frames, const uint64_t position, const float **const out,
    const uint64_t **const positions, uint32_t *const count)
{
    *out = nullptr;
    *positions = nullptr;
    *count = 0;
    uint32_t total = 0;
    if (input == nullptr) {
        if (resampler->context != nullptr && resampler->input_frames != 0) {
            MUST(M_Flush(resampler, &total));
        }
    } else {
        for (uint32_t offset = 0; offset < frames;
             offset += RESAMPLER_PIECE_FRAMES) {
            const uint32_t piece = MIN(frames - offset, RESAMPLER_PIECE_FRAMES);
            for (uint32_t i = 0; i < piece; i++) {
                resampler->history
                    [(resampler->input_frames + i) % RESAMPLER_HISTORY_FRAMES] =
                    position + offset + i;
            }
            MUST(M_Run(
                resampler, input + (size_t)offset * RESAMPLER_CHANNELS, piece,
                &total));
        }
    }
    *out = resampler->buffer;
    *positions = resampler->positions;
    *count = total;
    return OK;
}

RESULT AudioResampler_Reset(AUDIO_RESAMPLER *const resampler)
{
    resampler->input_frames = 0;
    resampler->output_frames = 0;
    if (resampler->context == nullptr) {
        return OK;
    }
    swr_close(resampler->context);
    const int error = swr_init(resampler->context);
    return error < 0 ? M_Error("cannot reset audio resampling", error) : OK;
}
