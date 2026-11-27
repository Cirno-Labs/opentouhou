#include "core/audio/resampler.h"

#include "core/audio/backend.h"
#include "core/base/memory.h"

#include "test.h"

#include <math.h>

#define TAU 6.283185307179586

static AUDIO_RESAMPLER *M_Open(const uint32_t rate)
{
    AUDIO_RESAMPLER *resampler = nullptr;
    CHECK(Result_Log(AudioResampler_Open(rate, &resampler), LOG_LEVEL_ERROR));
    return resampler;
}

// Returns frames stereo frames of a 1 kHz sine at rate, the right channel
// at half the level of the left.
static float *M_Sine(const uint32_t frames, const uint32_t rate)
{
    float *const data = Mem_AllocArr(frames, 2 * sizeof(float));
    for (uint32_t i = 0; i < frames; i++) {
        const double value = 0.5 * sin(TAU * 1000.0 * i / rate);
        data[i * 2] = (float)value;
        data[i * 2 + 1] = (float)(value * 0.5);
    }
    return data;
}

TEST(output_rate_passes_frames_and_positions_through)
{
    AUDIO_RESAMPLER *const resampler = M_Open(AUDIO_OUTPUT_RATE);
    float *const input = M_Sine(3000, AUDIO_OUTPUT_RATE);
    const float *output;
    const uint64_t *positions;
    uint32_t count;
    CHECK(Result_Log(
        AudioResampler_Convert(
            resampler, input, 3000, 100, &output, &positions, &count),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(count, 3000);
    for (uint32_t i = 0; i < count; i++) {
        CHECK(output[i * 2] == input[i * 2]);
        CHECK(output[i * 2 + 1] == input[i * 2 + 1]);
        CHECK_EQ_INT(positions[i], 100 + i);
    }
    CHECK(Result_Log(
        AudioResampler_Convert(
            resampler, nullptr, 0, 0, &output, &positions, &count),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(count, 0);
    Mem_Free(input);
    AudioResampler_Free(resampler);
}

TEST(second_at_44100_hz_gives_a_second_at_the_output_rate)
{
    AUDIO_RESAMPLER *const resampler = M_Open(44100);
    float *const input = M_Sine(44100, 44100);
    uint32_t total = 0;
    for (uint32_t at = 0; at < 44100; at += 2205) {
        const float *output;
        const uint64_t *positions;
        uint32_t count;
        CHECK(Result_Log(
            AudioResampler_Convert(
                resampler, input + at * 2, 2205, at, &output, &positions,
                &count),
            LOG_LEVEL_ERROR));
        for (uint32_t i = 0; i < count; i++) {
            CHECK_EQ_INT(
                positions[i],
                (uint64_t)(total + i) * 44100 / AUDIO_OUTPUT_RATE);
        }
        total += count;
    }
    const float *output;
    const uint64_t *positions;
    uint32_t count;
    CHECK(Result_Log(
        AudioResampler_Convert(
            resampler, nullptr, 0, 0, &output, &positions, &count),
        LOG_LEVEL_ERROR));
    CHECK(count > 0);
    for (uint32_t i = 0; i < count; i++) {
        CHECK(positions[i] <= 44099);
    }
    total += count;
    CHECK_EQ_INT(total, 48000);
    Mem_Free(input);
    AudioResampler_Free(resampler);
}

TEST(short_input_flushes_to_its_whole_length)
{
    const uint32_t rates[] = { 24000, 96000 };
    const uint32_t lengths[] = { 16, 8 };
    for (size_t i = 0; i < 2; i++) {
        AUDIO_RESAMPLER *const resampler = M_Open(rates[i]);
        float *const input = M_Sine(16, rates[i]);
        const float *output;
        const uint64_t *positions;
        uint32_t count;
        CHECK(Result_Log(
            AudioResampler_Convert(
                resampler, input, 8 * (i + 1), 0, &output, &positions, &count),
            LOG_LEVEL_ERROR));
        uint32_t total = count;
        CHECK(Result_Log(
            AudioResampler_Convert(
                resampler, nullptr, 0, 0, &output, &positions, &count),
            LOG_LEVEL_ERROR));
        total += count;
        CHECK_EQ_INT(total, lengths[i]);
        Mem_Free(input);
        AudioResampler_Free(resampler);
    }
}

TEST(positions_follow_a_jump_in_the_source)
{
    AUDIO_RESAMPLER *const resampler = M_Open(24000);
    float *const input = M_Sine(1000, 24000);
    const float *output;
    const uint64_t *positions;
    uint32_t count;
    CHECK(Result_Log(
        AudioResampler_Convert(
            resampler, input, 500, 0, &output, &positions, &count),
        LOG_LEVEL_ERROR));
    uint32_t total = count;
    CHECK(Result_Log(
        AudioResampler_Convert(
            resampler, input + 500 * 2, 500, 2000, &output, &positions, &count),
        LOG_LEVEL_ERROR));
    CHECK(count > 0);
    for (uint32_t i = 0; i < count; i++) {
        const uint64_t at = (uint64_t)(total + i) / 2;
        CHECK_EQ_INT(positions[i], at < 500 ? at : 2000 + at - 500);
    }
    Mem_Free(input);
    AudioResampler_Free(resampler);
}

TEST(reset_starts_a_new_signal)
{
    AUDIO_RESAMPLER *const resampler = M_Open(44100);
    float *const input = M_Sine(4410, 44100);
    const float *output;
    const uint64_t *positions;
    uint32_t count;
    CHECK(Result_Log(
        AudioResampler_Convert(
            resampler, input, 4410, 0, &output, &positions, &count),
        LOG_LEVEL_ERROR));
    const uint32_t first = count;
    CHECK(first > 0);
    const float start = output[0];
    CHECK(Result_Log(AudioResampler_Reset(resampler), LOG_LEVEL_ERROR));
    CHECK(Result_Log(
        AudioResampler_Convert(
            resampler, input, 4410, 7, &output, &positions, &count),
        LOG_LEVEL_ERROR));
    CHECK_EQ_INT(count, first);
    CHECK(output[0] == start);
    CHECK_EQ_INT(positions[0], 7);
    Mem_Free(input);
    AudioResampler_Free(resampler);
}

TEST(rejects_a_zero_rate)
{
    AUDIO_RESAMPLER *resampler = nullptr;
    CHECK(!Result_Ignore(AudioResampler_Open(0, &resampler)));
    CHECK(resampler == nullptr);
}
