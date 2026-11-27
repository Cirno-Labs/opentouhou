#include "core/audio.h"

#include "core/audio/backend.h"
#include "core/base/memory.h"
#include "core/base/utils.h"
#include "core/io/writer.h"

#include "test.h"

#include <SDL3/SDL.h>
#include <float.h>
#include <math.h>
#include <stdatomic.h>

#define WAIT_MILLISECONDS 5000
#define TAU 6.283185307179586

typedef struct {
    atomic_uint_fast64_t released_by;
} RELEASE_TRACKER;

static void M_CheckResult(RESULT result)
{
    CHECK(Result_Log(result, LOG_LEVEL_ERROR));
}

static void M_CheckNear(const float actual, const float expected)
{
    if (!isfinite(actual) || fabsf(actual - expected) > 0.00001f) {
        TEST_FAIL("expected %.8f, got %.8f", expected, actual);
    }
}

static void M_Mix(float *const output, const uint32_t frames)
{
    const Uint64 deadline = SDL_GetTicks() + WAIT_MILLISECONDS;
    while (!AudioBackend_Mix(output, frames)) {
        if (SDL_GetTicks() >= deadline) {
            TEST_FAIL("audio mixer did not acquire its state lock");
            return;
        }
        SDL_Delay(1);
    }
}

static float M_Left(const uint32_t frame)
{
    return (float)(frame % 8 + 1) / 32.0f;
}

static float M_Right(const uint32_t frame)
{
    return -(float)(frame % 8 + 1) / 64.0f;
}

static uint8_t *M_Wave(
    const uint32_t frames, const uint32_t sample_rate, size_t *const size)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Writer_WriteBytes(&writer, "RIFF", 4);
    Writer_WriteU32(&writer, 36 + frames * 4);
    Writer_WriteBytes(&writer, "WAVEfmt ", 8);
    Writer_WriteU32(&writer, 16);
    Writer_WriteU16(&writer, 1);
    Writer_WriteU16(&writer, 2);
    Writer_WriteU32(&writer, sample_rate);
    Writer_WriteU32(&writer, sample_rate * 4);
    Writer_WriteU16(&writer, 4);
    Writer_WriteU16(&writer, 16);
    Writer_WriteBytes(&writer, "data", 4);
    Writer_WriteU32(&writer, frames * 4);
    for (uint32_t i = 0; i < frames; i++) {
        Writer_WriteI16(&writer, (int16_t)((i % 8 + 1) * 1024));
        Writer_WriteI16(&writer, (int16_t)(-(int32_t)(i % 8 + 1) * 512));
    }
    uint8_t *const data = Writer_TakeBuffer(&writer, size);
    M_CheckResult(Writer_Close(&writer));
    return data;
}

static READER M_WaveReader(const uint32_t frames)
{
    size_t size;
    uint8_t *const data = M_Wave(frames, 48000, &size);
    READER reader;
    Reader_OpenOwnedBuffer(&reader, data, size, &g_HeapAllocator, nullptr);
    return reader;
}

// Returns an owned reader over pad bytes of zeros and then frames sample
// frames of raw PCM with the values of M_Wave, or only their left channel.
static READER M_PcmReader(
    const uint32_t frames, const uint32_t channels, const size_t pad)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    for (size_t i = 0; i < pad; i++) {
        Writer_WriteU8(&writer, 0);
    }
    for (uint32_t i = 0; i < frames; i++) {
        Writer_WriteI16(&writer, (int16_t)((i % 8 + 1) * 1024));
        if (channels == 2) {
            Writer_WriteI16(&writer, (int16_t)(-(int32_t)(i % 8 + 1) * 512));
        }
    }
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    M_CheckResult(Writer_Close(&writer));
    READER reader;
    Reader_OpenOwnedBuffer(&reader, data, size, &g_HeapAllocator, nullptr);
    return reader;
}

static void M_RecordFree(
    void *const context, void *const memory, const size_t size)
{
    RELEASE_TRACKER *const tracker = context;
    atomic_store(&tracker->released_by, SDL_GetCurrentThreadID());
    Mem_Free(memory);
}

static void M_WaitSample(const AUDIO_SAMPLE *const sample)
{
    const Uint64 deadline = SDL_GetTicks() + WAIT_MILLISECONDS;
    while (AudioSample_GetState(sample) == AUDIO_LOADING
           && SDL_GetTicks() < deadline) {
        SDL_Delay(1);
    }
    CHECK(AudioSample_GetState(sample) != AUDIO_LOADING);
}

static void M_WaitStream(const AUDIO_STREAM *const stream)
{
    const Uint64 deadline = SDL_GetTicks() + WAIT_MILLISECONDS;
    while (AudioStream_GetState(stream) == AUDIO_LOADING
           && SDL_GetTicks() < deadline) {
        SDL_Delay(1);
    }
    CHECK(AudioStream_GetState(stream) != AUDIO_LOADING);
}

static AUDIO_PLAY_OPTIONS M_Options(void)
{
    return (AUDIO_PLAY_OPTIONS) {
        .group = AUDIO_GROUP_EFFECTS,
        .volume = 1.0f,
        .pitch = 1.0f,
    };
}

static AUDIO_SAMPLE *M_LoadSample(const uint32_t frames)
{
    READER reader = M_WaveReader(frames);
    AUDIO_SAMPLE *sample = nullptr;
    M_CheckResult(AudioSample_Load(&reader, &sample));
    Reader_Close(&reader);
    if (sample != nullptr) {
        M_WaitSample(sample);
        M_CheckResult(AudioSample_GetResult(sample));
    }
    return sample;
}

static AUDIO_PLAYBACK *M_Play(
    AUDIO_SAMPLE *const sample, const AUDIO_PLAY_OPTIONS *const options)
{
    AUDIO_PLAYBACK *playback = nullptr;
    M_CheckResult(AudioSample_Play(sample, options, &playback));
    return playback;
}

static AUDIO_STREAM *M_LoadStream(
    const uint32_t frames, const AUDIO_PLAY_OPTIONS *const options)
{
    READER reader = M_WaveReader(frames);
    AUDIO_STREAM *stream = nullptr;
    M_CheckResult(AudioStream_Load(&reader, options, &stream));
    Reader_Close(&reader);
    if (stream != nullptr) {
        M_WaitStream(stream);
        M_CheckResult(AudioStream_GetResult(stream));
    }
    return stream;
}

static AUDIO_STREAM *M_LoadPcmStream(
    const uint32_t frames, const AUDIO_PLAY_OPTIONS *const options)
{
    READER reader = M_PcmReader(frames, 2, 0);
    AUDIO_STREAM *stream = nullptr;
    const AUDIO_PCM_FORMAT format = { .sample_rate = 48000, .channels = 2 };
    M_CheckResult(AudioStream_LoadPcm(&reader, format, options, &stream));
    Reader_Close(&reader);
    if (stream != nullptr) {
        M_WaitStream(stream);
        M_CheckResult(AudioStream_GetResult(stream));
    }
    return stream;
}

// Writes frames mono frames of a sine of frequency hertz at rate, with
// amplitude 0.4 about 0.5, so that no frame is silent.
static void M_WriteSine(
    WRITER *const writer, const uint32_t frames, const uint32_t rate,
    const double frequency)
{
    for (uint32_t i = 0; i < frames; i++) {
        const double value = 0.5 + 0.4 * sin(TAU * frequency * i / rate);
        Writer_WriteI16(writer, (int16_t)lround(value * 32767.0));
    }
}

// Writes a mono WAV file of a sine of M_WriteSine.
static void M_WriteSineWave(
    WRITER *const writer, const uint32_t frames, const uint32_t rate,
    const double frequency)
{
    Writer_WriteBytes(writer, "RIFF", 4);
    Writer_WriteU32(writer, 36 + frames * 2);
    Writer_WriteBytes(writer, "WAVEfmt ", 8);
    Writer_WriteU32(writer, 16);
    Writer_WriteU16(writer, 1);
    Writer_WriteU16(writer, 1);
    Writer_WriteU32(writer, rate);
    Writer_WriteU32(writer, rate * 2);
    Writer_WriteU16(writer, 2);
    Writer_WriteU16(writer, 16);
    Writer_WriteBytes(writer, "data", 4);
    Writer_WriteU32(writer, frames * 2);
    M_WriteSine(writer, frames, rate, frequency);
}

static READER M_TakeReader(WRITER *const writer)
{
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(writer, &size);
    M_CheckResult(Writer_Close(writer));
    READER reader;
    Reader_OpenOwnedBuffer(&reader, data, size, &g_HeapAllocator, nullptr);
    return reader;
}

// Loads a stream of a sine at 44100 Hz from raw PCM or from a WAV file.
static AUDIO_STREAM *M_LoadSineStream(
    const uint32_t frames, const double frequency,
    const AUDIO_PLAY_OPTIONS *const options, const bool raw)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    if (raw) {
        M_WriteSine(&writer, frames, 44100, frequency);
    } else {
        M_WriteSineWave(&writer, frames, 44100, frequency);
    }
    READER reader = M_TakeReader(&writer);
    AUDIO_STREAM *stream = nullptr;
    if (raw) {
        const AUDIO_PCM_FORMAT format = { .sample_rate = 44100, .channels = 1 };
        M_CheckResult(AudioStream_LoadPcm(&reader, format, options, &stream));
    } else {
        M_CheckResult(AudioStream_Load(&reader, options, &stream));
    }
    Reader_Close(&reader);
    if (stream != nullptr) {
        M_WaitStream(stream);
        M_CheckResult(AudioStream_GetResult(stream));
    }
    return stream;
}

// Checks that frames [from, to) of a sine of M_WriteSine keep its amplitude
// within 2.5% and its frequency within 1% at the output rate.
static void M_CheckSine(
    const float *const frames, const uint32_t from, const uint32_t to,
    const double frequency)
{
    double sum = 0.0;
    for (uint32_t i = from; i < to; i++) {
        sum += frames[i];
    }
    const double mean = sum / (to - from);
    double power = 0.0;
    uint32_t crossings = 0;
    for (uint32_t i = from; i < to; i++) {
        power += (frames[i] - mean) * (frames[i] - mean);
        if (i > from && (frames[i - 1] < mean) != (frames[i] < mean)) {
            crossings++;
        }
    }
    const double amplitude = sqrt(2.0 * power / (to - from));
    if (amplitude < 0.39 || amplitude > 0.41) {
        TEST_FAIL("%.0f Hz has amplitude %.4f, not 0.4", frequency, amplitude);
    }
    const double expected = 2.0 * frequency * (to - from) / 48000.0;
    if (fabs(crossings - expected) > expected * 0.01) {
        TEST_FAIL(
            "%.0f Hz crosses its mean %u times, not %.0f", frequency, crossings,
            expected);
    }
}

// Mixes stream until frames frames play, and writes their left channel to
// out. Skips the frames that an underrun leaves silent.
static void M_MixPlayed(
    AUDIO_STREAM *const stream, float *const out, const uint32_t frames)
{
    const Uint64 deadline = SDL_GetTicks() + WAIT_MILLISECONDS;
    uint32_t count = 0;
    float chunk[1024 * 2];
    while (count < frames) {
        if (SDL_GetTicks() >= deadline) {
            TEST_FAIL("the stream played %u frames, not %u", count, frames);
            break;
        }
        const uint32_t size = MIN(frames - count, 1024);
        M_Mix(chunk, size);
        for (uint32_t i = 0; i < size; i++) {
            if (chunk[i * 2] != 0.0f) {
                out[count++] = chunk[i * 2];
            }
        }
        SDL_Delay(1);
    }
}

// Mixes stream until it finishes, and writes the left channel of the
// frames that it plays to out. Returns their count. Skips the frames that
// an underrun leaves silent, so the stream must have no silent frame.
static uint32_t M_MixToEnd(
    AUDIO_STREAM *const stream, float *const out, const uint32_t capacity)
{
    const Uint64 deadline = SDL_GetTicks() + WAIT_MILLISECONDS;
    uint32_t count = 0;
    float chunk[1024 * 2];
    while (!AudioStream_IsFinished(stream)) {
        if (SDL_GetTicks() >= deadline) {
            TEST_FAIL("the stream did not finish");
            break;
        }
        memset(chunk, 0, sizeof(chunk));
        M_Mix(chunk, 1024);
        uint32_t played = 0;
        for (uint32_t i = 0; i < 1024; i++) {
            if (chunk[i * 2] != 0.0f) {
                if (count < capacity) {
                    out[count] = chunk[i * 2];
                }
                count++;
                played++;
            }
        }
        if (played < 1024) {
            SDL_Delay(1);
        }
    }
    return count;
}

TEST(sample_load_transfers_reader_and_decodes_off_main_thread)
{
    M_CheckResult(Audio_Init());
    RELEASE_TRACKER tracker = {};
    const ALLOCATOR allocator = {
        .free = M_RecordFree,
        .context = &tracker,
    };
    size_t size;
    uint8_t *const data = M_Wave(16, 48000, &size);
    READER reader;
    Reader_OpenOwnedBuffer(&reader, data, size, &allocator, nullptr);
    AUDIO_SAMPLE *sample = nullptr;
    M_CheckResult(AudioSample_Load(&reader, &sample));
    CHECK_EQ_INT(Reader_Size(&reader), 0);
    CHECK(reader.data == nullptr);
    M_WaitSample(sample);
    CHECK_EQ_INT(AudioSample_GetState(sample), AUDIO_READY);
    M_CheckResult(AudioSample_GetResult(sample));
    AUDIO_INFO info;
    M_CheckResult(AudioSample_GetInfo(sample, &info));
    CHECK_EQ_INT(info.sample_rate, 48000);
    CHECK_EQ_INT(info.channels, 2);
    CHECK(info.length_known);
    CHECK_EQ_INT(info.length, 16);
    AudioSample_Free(sample);
    Audio_Shutdown();
    const uint64_t released_by = atomic_load(&tracker.released_by);
    CHECK(released_by != 0);
    CHECK(released_by != SDL_GetCurrentThreadID());
}

TEST(canceled_loads_release_owned_readers_before_free_returns)
{
    M_CheckResult(Audio_Init());
    for (int i = 0; i < 8; i++) {
        RELEASE_TRACKER tracker = {};
        const ALLOCATOR allocator = {
            .free = M_RecordFree,
            .context = &tracker,
        };
        size_t size;
        uint8_t *data = M_Wave(65536, 48000, &size);
        READER reader;
        Reader_OpenOwnedBuffer(&reader, data, size, &allocator, nullptr);
        AUDIO_SAMPLE *sample = nullptr;
        M_CheckResult(AudioSample_Load(&reader, &sample));
        AudioSample_Free(sample);
        CHECK(atomic_load(&tracker.released_by) != 0);
        CHECK(atomic_load(&tracker.released_by) != SDL_GetCurrentThreadID());
        atomic_store(&tracker.released_by, 0);
        data = M_Wave(65536, 48000, &size);
        Reader_OpenOwnedBuffer(&reader, data, size, &allocator, nullptr);
        AUDIO_PLAY_OPTIONS options = M_Options();
        AUDIO_STREAM *stream = nullptr;
        M_CheckResult(AudioStream_Load(&reader, &options, &stream));
        AudioStream_Free(stream);
        CHECK(atomic_load(&tracker.released_by) != 0);
        CHECK(atomic_load(&tracker.released_by) != SDL_GetCurrentThreadID());
    }
    Audio_Shutdown();
}

TEST(sample_load_rejects_borrowed_reader_and_slice)
{
    M_CheckResult(Audio_Init());
    size_t size;
    uint8_t *const data = M_Wave(8, 48000, &size);
    READER borrowed;
    Reader_OpenBuffer(&borrowed, data, size, nullptr);
    AUDIO_SAMPLE *sample = nullptr;
    CHECK(!Result_Ignore(AudioSample_Load(&borrowed, &sample)));
    CHECK_EQ_INT(Reader_Size(&borrowed), size);
    CHECK(sample == nullptr);
    READER owned;
    Reader_OpenOwnedBuffer(&owned, data, size, &g_HeapAllocator, nullptr);
    READER slice = Reader_Slice(&owned, size);
    CHECK(!Result_Ignore(AudioSample_Load(&slice, &sample)));
    CHECK_EQ_INT(Reader_Size(&slice), size);
    Reader_Close(&owned);
    Audio_Shutdown();
}

TEST(sample_load_reports_async_failure_repeatedly)
{
    M_CheckResult(Audio_Init());
    READER reader;
    Reader_OpenOwnedBuffer(
        &reader, Mem_Dup("invalid audio", 13), 13, &g_HeapAllocator, nullptr);
    AUDIO_SAMPLE *sample = nullptr;
    M_CheckResult(AudioSample_Load(&reader, &sample));
    M_WaitSample(sample);
    CHECK_EQ_INT(AudioSample_GetState(sample), AUDIO_FAILED);
    CHECK(!Result_Ignore(AudioSample_GetResult(sample)));
    CHECK(!Result_Ignore(AudioSample_GetResult(sample)));
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_PLAYBACK *playback = nullptr;
    CHECK(!Result_Ignore(AudioSample_Play(sample, &options, &playback)));
    CHECK(playback == nullptr);
    AudioSample_Free(sample);
    Audio_Shutdown();
}

TEST(missing_file_is_an_accepted_async_load)
{
    M_CheckResult(Audio_Init());
    AUDIO_SAMPLE *sample = nullptr;
    M_CheckResult(AudioSample_LoadFile(
        "audio-test-file-that-does-not-exist.wav", &sample));
    M_WaitSample(sample);
    CHECK_EQ_INT(AudioSample_GetState(sample), AUDIO_FAILED);
    CHECK(!Result_Ignore(AudioSample_GetResult(sample)));
    AudioSample_Free(sample);
    Audio_Shutdown();
}

TEST(sample_playbacks_keep_data_after_sample_is_freed)
{
    M_CheckResult(Audio_Init());
    AUDIO_SAMPLE *const sample = M_LoadSample(8);
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_PLAYBACK *const first = M_Play(sample, &options);
    float output[6];
    M_Mix(output, 1);
    M_CheckNear(output[0], M_Left(0));
    M_CheckNear(output[1], M_Right(0));
    AUDIO_PLAYBACK *const second = M_Play(sample, &options);
    AudioSample_Free(sample);
    M_Mix(output, 1);
    M_CheckNear(output[0], M_Left(1) + M_Left(0));
    M_CheckNear(output[1], M_Right(1) + M_Right(0));
    AudioPlayback_SetPaused(first, true);
    M_Mix(output, 1);
    M_CheckNear(output[0], M_Left(1));
    AudioPlayback_Stop(second);
    CHECK(AudioPlayback_IsFinished(second));
    AudioPlayback_SetPaused(first, false);
    M_Mix(output, 1);
    M_CheckNear(output[0], M_Left(2));
    AudioPlayback_Free(first);
    AudioPlayback_Free(second);
    Audio_Shutdown();
}

TEST(sample_loop_plays_intro_once_and_restarts_at_zero)
{
    M_CheckResult(Audio_Init());
    AUDIO_SAMPLE *const sample = M_LoadSample(8);
    AUDIO_PLAY_OPTIONS options = M_Options();
    options.loop = (AUDIO_LOOP) { .enabled = true, .begin = 1, .end = 3 };
    AUDIO_PLAYBACK *const playback = M_Play(sample, &options);
    float output[14];
    M_Mix(output, 7);
    const uint32_t expected[] = { 0, 1, 2, 1, 2, 1, 2 };
    for (size_t i = 0; i < 7; i++) {
        M_CheckNear(output[i * 2], M_Left(expected[i]));
        M_CheckNear(output[i * 2 + 1], M_Right(expected[i]));
    }
    CHECK(!AudioPlayback_IsFinished(playback));
    AudioPlayback_Restart(playback);
    M_Mix(output, 1);
    M_CheckNear(output[0], M_Left(0));
    AudioPlayback_Free(playback);
    AudioSample_Free(sample);
    Audio_Shutdown();
}

TEST(sample_gain_balance_and_pitch_are_independent)
{
    M_CheckResult(Audio_Init());
    AUDIO_SAMPLE *const sample = M_LoadSample(8);
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_PLAYBACK *const playback = M_Play(sample, &options);
    M_CheckResult(AudioPlayback_SetVolume(playback, 0.5f));
    M_CheckResult(AudioPlayback_SetPan(playback, -1.0f));
    M_CheckResult(AudioPlayback_SetPitch(playback, 2.0f));
    float output[6];
    M_Mix(output, 3);
    for (uint32_t i = 0; i < 3; i++) {
        M_CheckNear(output[i * 2], M_Left(i * 2) * 0.5f);
        M_CheckNear(output[i * 2 + 1], 0.0f);
    }
    CHECK(!Result_Ignore(AudioPlayback_SetVolume(playback, -1.0f)));
    CHECK(!Result_Ignore(AudioPlayback_SetPan(playback, 2.0f)));
    CHECK(!Result_Ignore(AudioPlayback_SetPitch(playback, 0.0f)));
    CHECK(!Result_Ignore(AudioPlayback_SetVolume(playback, NAN)));
    CHECK(!Result_Ignore(AudioPlayback_SetVolume(playback, INFINITY)));
    CHECK(!Result_Ignore(AudioPlayback_SetPan(playback, NAN)));
    CHECK(!Result_Ignore(AudioPlayback_SetPan(playback, INFINITY)));
    CHECK(!Result_Ignore(AudioPlayback_SetPitch(playback, NAN)));
    CHECK(!Result_Ignore(AudioPlayback_SetPitch(playback, INFINITY)));
    AudioPlayback_Free(playback);
    AudioSample_Free(sample);
    Audio_Shutdown();
}

TEST(source_sample_rate_controls_output_playback_speed)
{
    M_CheckResult(Audio_Init());
    size_t size;
    uint8_t *const data = M_Wave(8, 24000, &size);
    READER reader;
    Reader_OpenOwnedBuffer(&reader, data, size, &g_HeapAllocator, nullptr);
    AUDIO_SAMPLE *sample = nullptr;
    M_CheckResult(AudioSample_Load(&reader, &sample));
    M_WaitSample(sample);
    M_CheckResult(AudioSample_GetResult(sample));
    AUDIO_INFO info;
    M_CheckResult(AudioSample_GetInfo(sample, &info));
    CHECK_EQ_INT(info.sample_rate, 24000);
    CHECK_EQ_INT(info.length, 8);
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_PLAYBACK *const playback = M_Play(sample, &options);
    // Plays 8 frames at 24000 Hz in 16 frames at 48000 Hz.
    float output[32];
    M_Mix(output, 15);
    CHECK(!AudioPlayback_IsFinished(playback));
    M_Mix(output, 1);
    CHECK(AudioPlayback_IsFinished(playback));
    AudioPlayback_Free(playback);
    AudioSample_Free(sample);
    Audio_Shutdown();
}

TEST(sample_at_44100_hz_keeps_its_high_frequencies)
{
    M_CheckResult(Audio_Init());
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    M_WriteSineWave(&writer, 4410, 44100, 15000.0);
    READER reader = M_TakeReader(&writer);
    AUDIO_SAMPLE *sample = nullptr;
    M_CheckResult(AudioSample_Load(&reader, &sample));
    M_WaitSample(sample);
    M_CheckResult(AudioSample_GetResult(sample));
    AUDIO_INFO info;
    M_CheckResult(AudioSample_GetInfo(sample, &info));
    CHECK_EQ_INT(info.length, 4410);
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_PLAYBACK *const playback = M_Play(sample, &options);
    float output[4800 * 2];
    M_Mix(output, 4800);
    float left[4800];
    for (uint32_t i = 0; i < 4800; i++) {
        left[i] = output[i * 2];
    }
    M_CheckSine(left, 200, 4600, 15000.0);
    AudioPlayback_Free(playback);
    AudioSample_Free(sample);
    Audio_Shutdown();
}

TEST(group_mute_advances_position_and_group_pause_preserves_it)
{
    M_CheckResult(Audio_Init());
    AUDIO_SAMPLE *const sample = M_LoadSample(8);
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_PLAYBACK *const playback = M_Play(sample, &options);
    float output[4];
    Audio_SetGroupMuted(AUDIO_GROUP_EFFECTS, true);
    M_Mix(output, 2);
    M_CheckNear(output[0], 0.0f);
    M_CheckNear(output[3], 0.0f);
    Audio_SetGroupMuted(AUDIO_GROUP_EFFECTS, false);
    Audio_SetGroupPaused(AUDIO_GROUP_EFFECTS, true);
    M_Mix(output, 2);
    M_CheckNear(output[0], 0.0f);
    CHECK(!AudioPlayback_IsFinished(playback));
    Audio_SetGroupPaused(AUDIO_GROUP_EFFECTS, false);
    M_CheckResult(Audio_SetMasterVolume(0.5f));
    M_CheckResult(Audio_SetGroupVolume(AUDIO_GROUP_EFFECTS, 0.5f));
    M_Mix(output, 1);
    M_CheckNear(output[0], M_Left(2) * 0.25f);
    M_CheckNear(output[1], M_Right(2) * 0.25f);
    AudioPlayback_Free(playback);
    AudioSample_Free(sample);
    Audio_Shutdown();
}

TEST(sample_end_can_be_restarted)
{
    M_CheckResult(Audio_Init());
    AUDIO_SAMPLE *const sample = M_LoadSample(2);
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_PLAYBACK *const playback = M_Play(sample, &options);
    float output[8];
    M_Mix(output, 4);
    CHECK(AudioPlayback_IsFinished(playback));
    M_CheckNear(output[0], M_Left(0));
    M_CheckNear(output[2], M_Left(1));
    M_CheckNear(output[4], 0.0f);
    AudioPlayback_Restart(playback);
    M_Mix(output, 1);
    CHECK(!AudioPlayback_IsFinished(playback));
    M_CheckNear(output[0], M_Left(0));
    M_CheckResult(AudioPlayback_SetPitch(playback, FLT_MAX));
    M_Mix(output, 2);
    M_CheckNear(output[0], M_Left(1));
    M_CheckNear(output[2], 0.0f);
    CHECK(AudioPlayback_IsFinished(playback));
    AudioPlayback_Free(playback);
    AudioSample_Free(sample);
    Audio_Shutdown();
}

TEST(stream_starts_paused_and_seek_requires_explicit_play)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    options.group = AUDIO_GROUP_MUSIC;
    AUDIO_STREAM *const stream = M_LoadStream(32, &options);
    AUDIO_INFO info;
    M_CheckResult(AudioStream_GetInfo(stream, &info));
    CHECK_EQ_INT(info.sample_rate, 48000);
    CHECK_EQ_INT(info.length, 32);
    float output[4];
    M_Mix(output, 1);
    M_CheckNear(output[0], 0.0f);
    CHECK_EQ_INT(AudioStream_GetPosition(stream), 0);
    M_CheckResult(AudioStream_Play(stream));
    M_Mix(output, 2);
    M_CheckNear(output[0], M_Left(0));
    M_CheckNear(output[2], M_Left(1));
    CHECK_EQ_INT(AudioStream_GetPosition(stream), 2);
    M_CheckResult(AudioStream_Seek(stream, 5));
    M_WaitStream(stream);
    M_CheckResult(AudioStream_GetResult(stream));
    M_Mix(output, 1);
    M_CheckNear(output[0], 0.0f);
    CHECK_EQ_INT(AudioStream_GetPosition(stream), 5);
    M_CheckResult(AudioStream_Play(stream));
    M_Mix(output, 1);
    M_CheckNear(output[0], M_Left(5));
    M_CheckNear(output[1], M_Right(5));
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(stream_latest_seek_replaces_pending_positions)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_STREAM *const stream = M_LoadStream(32, &options);
    for (int i = 0; i < 32; i++) {
        M_CheckResult(AudioStream_Seek(stream, 5));
        M_CheckResult(AudioStream_Seek(stream, 1));
        M_CheckResult(AudioStream_Seek(stream, 7));
    }
    M_WaitStream(stream);
    M_CheckResult(AudioStream_GetResult(stream));
    CHECK_EQ_INT(AudioStream_GetPosition(stream), 7);
    M_CheckResult(AudioStream_Play(stream));
    float output[2];
    M_Mix(output, 1);
    M_CheckNear(output[0], M_Left(7));
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(stream_loop_repeats_exact_source_range)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    options.loop = (AUDIO_LOOP) { .enabled = true, .begin = 1, .end = 3 };
    AUDIO_STREAM *const stream = M_LoadStream(16, &options);
    M_CheckResult(AudioStream_Play(stream));
    float output[14];
    M_Mix(output, 7);
    const uint32_t expected[] = { 0, 1, 2, 1, 2, 1, 2 };
    for (size_t i = 0; i < 7; i++) {
        M_CheckNear(output[i * 2], M_Left(expected[i]));
        M_CheckNear(output[i * 2 + 1], M_Right(expected[i]));
    }
    CHECK(!AudioStream_IsFinished(stream));
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(stream_loop_changes_remain_valid_during_background_refill)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    READER reader = M_WaveReader(65536);
    AUDIO_STREAM *stream = nullptr;
    M_CheckResult(AudioStream_Load(&reader, &options, &stream));
    for (uint64_t i = 0; i < 96; i++) {
        const AUDIO_LOOP loop = {
            .enabled = true,
            .begin = i % 8,
            .end = 16 + i % 8,
        };
        M_CheckResult(AudioStream_SetLoop(stream, loop));
    }
    M_WaitStream(stream);
    M_CheckResult(AudioStream_GetResult(stream));
    M_CheckResult(AudioStream_Play(stream));
    float output[128];
    for (uint64_t i = 0; i < 96; i++) {
        const AUDIO_LOOP loop = {
            .enabled = true,
            .begin = i % 8,
            .end = 16 + i % 8,
        };
        M_CheckResult(AudioStream_SetLoop(stream, loop));
        M_Mix(output, 64);
        for (size_t at = 0; at < 64; at++) {
            CHECK(isfinite(output[at * 2]));
            CHECK(output[at * 2] >= 0.0f && output[at * 2] <= 0.25f);
            M_CheckNear(output[at * 2 + 1], -output[at * 2] * 0.5f);
        }
        CHECK_EQ_INT(AudioStream_GetState(stream), AUDIO_READY);
        CHECK(!AudioStream_IsFinished(stream));
        SDL_Delay(1);
    }
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(stream_underrun_does_not_skip_source_frames)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_STREAM *const stream = M_LoadStream(200000, &options);
    M_CheckResult(AudioStream_Play(stream));
    const uint32_t frames = 400000;
    float *const output = Mem_AllocArr(frames * 2, sizeof(float));
    M_Mix(output, frames);
    uint64_t played = 0;
    for (uint32_t i = 0; i < frames; i++) {
        if (output[i * 2] != 0.0f) {
            M_CheckNear(output[i * 2], M_Left((uint32_t)played));
            M_CheckNear(output[i * 2 + 1], M_Right((uint32_t)played));
            played++;
        } else {
            M_CheckNear(output[i * 2 + 1], 0.0f);
        }
    }
    CHECK(played > 0);
    CHECK(played <= 200000);
    CHECK_EQ_INT(AudioStream_GetPosition(stream), played);
    Mem_Free(output);
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(stream_finishes_after_its_last_buffer_drains)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_STREAM *const stream = M_LoadStream(4096, &options);
    M_CheckResult(AudioStream_Play(stream));
    float output[8192];
    M_Mix(output, 4096);
    for (uint32_t i = 0; i < 4096; i++) {
        M_CheckNear(output[i * 2], M_Left(i));
    }
    const Uint64 deadline = SDL_GetTicks() + WAIT_MILLISECONDS;
    while (!AudioStream_IsFinished(stream) && SDL_GetTicks() < deadline) {
        SDL_Delay(1);
        M_Mix(output, 1);
        M_CheckNear(output[0], 0.0f);
    }
    CHECK(AudioStream_IsFinished(stream));
    CHECK_EQ_INT(AudioStream_GetPosition(stream), 4096);
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(stream_fade_stops_playback)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_STREAM *const stream = M_LoadStream(32, &options);
    M_CheckResult(AudioStream_Play(stream));
    M_CheckResult(AudioStream_Fade(stream, 0.0f, 4.0 / 48000.0, true));
    float output[16];
    M_Mix(output, 8);
    CHECK(output[0] > 0.0f);
    M_CheckNear(output[14], 0.0f);
    M_CheckNear(output[15], 0.0f);
    CHECK(AudioStream_IsFinished(stream));
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(empty_stream_finishes_or_reports_a_loading_failure)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    READER reader = M_WaveReader(0);
    AUDIO_STREAM *stream = nullptr;
    M_CheckResult(AudioStream_Load(&reader, &options, &stream));
    M_WaitStream(stream);
    if (AudioStream_GetState(stream) == AUDIO_READY) {
        M_CheckResult(AudioStream_Play(stream));
        float output[2];
        M_Mix(output, 1);
        M_CheckNear(output[0], 0.0f);
        CHECK(AudioStream_IsFinished(stream));
    } else {
        CHECK(!Result_Ignore(AudioStream_GetResult(stream)));
    }
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(stream_loop_beyond_source_end_reports_loading_failure)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    options.loop = (AUDIO_LOOP) { .enabled = true, .begin = 1, .end = 9 };
    READER reader = M_WaveReader(8);
    AUDIO_STREAM *stream = nullptr;
    M_CheckResult(AudioStream_Load(&reader, &options, &stream));
    M_WaitStream(stream);
    CHECK_EQ_INT(AudioStream_GetState(stream), AUDIO_FAILED);
    CHECK(!Result_Ignore(AudioStream_GetResult(stream)));
    CHECK(!Result_Ignore(AudioStream_GetResult(stream)));
    CHECK(!Result_Ignore(AudioStream_Play(stream)));
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(stream_rejects_invalid_controls_without_losing_ready_audio)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_STREAM *const stream = M_LoadStream(8, &options);
    CHECK(!Result_Ignore(AudioStream_SetVolume(stream, NAN)));
    CHECK(!Result_Ignore(AudioStream_SetVolume(stream, INFINITY)));
    CHECK(!Result_Ignore(AudioStream_SetPan(stream, 2.0f)));
    CHECK(!Result_Ignore(AudioStream_SetPitch(stream, 0.0f)));
    CHECK(!Result_Ignore(AudioStream_SetPitch(stream, NAN)));
    CHECK(!Result_Ignore(AudioStream_SetPitch(stream, INFINITY)));
    CHECK(!Result_Ignore(AudioStream_SetPitch(stream, FLT_MAX)));
    CHECK(!Result_Ignore(AudioStream_SetLoop(
        stream, (AUDIO_LOOP) { .enabled = true, .begin = 3, .end = 3 })));
    CHECK(!Result_Ignore(AudioStream_SetLoop(
        stream, (AUDIO_LOOP) { .enabled = true, .begin = 1, .end = 9 })));
    CHECK(!Result_Ignore(AudioStream_Seek(stream, 9)));
    CHECK(!Result_Ignore(AudioStream_Fade(stream, 0.0f, -1.0, true)));
    CHECK_EQ_INT(AudioStream_GetState(stream), AUDIO_READY);
    M_CheckResult(AudioStream_Play(stream));
    float output[2];
    M_Mix(output, 1);
    M_CheckNear(output[0], M_Left(0));
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(stream_missing_file_reports_async_failure)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_STREAM *stream = nullptr;
    M_CheckResult(AudioStream_LoadFile(
        "audio-test-file-that-does-not-exist.wav", &options, &stream));
    M_WaitStream(stream);
    CHECK_EQ_INT(AudioStream_GetState(stream), AUDIO_FAILED);
    CHECK(!Result_Ignore(AudioStream_GetResult(stream)));
    CHECK(!Result_Ignore(AudioStream_Play(stream)));
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(pcm_stream_plays_the_frames_of_its_reader)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_STREAM *const stream = M_LoadPcmStream(32, &options);
    AUDIO_INFO info;
    M_CheckResult(AudioStream_GetInfo(stream, &info));
    CHECK_EQ_INT(info.sample_rate, 48000);
    CHECK_EQ_INT(info.channels, 2);
    CHECK(info.length_known);
    CHECK_EQ_INT(info.length, 32);
    M_CheckResult(AudioStream_Play(stream));
    float output[64];
    M_Mix(output, 32);
    for (uint32_t i = 0; i < 32; i++) {
        M_CheckNear(output[i * 2], M_Left(i));
        M_CheckNear(output[i * 2 + 1], M_Right(i));
    }
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(pcm_stream_of_a_narrowed_reader_plays_only_its_range)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    READER reader = M_PcmReader(16, 2, 6);
    M_CheckResult(Reader_Narrow(&reader, 6 + 3 * 4, 4 * 4));
    AUDIO_STREAM *stream = nullptr;
    const AUDIO_PCM_FORMAT format = { .sample_rate = 48000, .channels = 2 };
    M_CheckResult(AudioStream_LoadPcm(&reader, format, &options, &stream));
    M_WaitStream(stream);
    M_CheckResult(AudioStream_GetResult(stream));
    M_CheckResult(AudioStream_Play(stream));
    float output[12];
    M_Mix(output, 6);
    for (uint32_t i = 0; i < 4; i++) {
        M_CheckNear(output[i * 2], M_Left(i + 3));
    }
    M_CheckNear(output[8], 0.0f);
    M_CheckNear(output[10], 0.0f);
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(pcm_stream_loop_repeats_exact_frames)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    options.loop = (AUDIO_LOOP) { .enabled = true, .begin = 2, .end = 5 };
    AUDIO_STREAM *const stream = M_LoadPcmStream(16, &options);
    M_CheckResult(AudioStream_Play(stream));
    float output[18];
    M_Mix(output, 9);
    const uint32_t expected[] = { 0, 1, 2, 3, 4, 2, 3, 4, 2 };
    for (size_t i = 0; i < 9; i++) {
        M_CheckNear(output[i * 2], M_Left(expected[i]));
        M_CheckNear(output[i * 2 + 1], M_Right(expected[i]));
    }
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(pcm_stream_seeks_to_a_frame)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_STREAM *const stream = M_LoadPcmStream(32, &options);
    M_CheckResult(AudioStream_Seek(stream, 13));
    M_WaitStream(stream);
    M_CheckResult(AudioStream_GetResult(stream));
    CHECK_EQ_INT(AudioStream_GetPosition(stream), 13);
    M_CheckResult(AudioStream_Play(stream));
    float output[4];
    M_Mix(output, 2);
    M_CheckNear(output[0], M_Left(13));
    M_CheckNear(output[2], M_Left(14));
    CHECK_EQ_INT(AudioStream_GetPosition(stream), 15);
    CHECK(!Result_Ignore(AudioStream_Seek(stream, 33)));
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(pcm_mono_plays_on_both_channels)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    READER reader = M_PcmReader(4, 1, 0);
    AUDIO_STREAM *stream = nullptr;
    const AUDIO_PCM_FORMAT format = { .sample_rate = 48000, .channels = 1 };
    M_CheckResult(AudioStream_LoadPcm(&reader, format, &options, &stream));
    M_WaitStream(stream);
    M_CheckResult(AudioStream_GetResult(stream));
    AUDIO_INFO info;
    M_CheckResult(AudioStream_GetInfo(stream, &info));
    CHECK_EQ_INT(info.length, 4);
    M_CheckResult(AudioStream_Play(stream));
    float output[8];
    M_Mix(output, 4);
    for (uint32_t i = 0; i < 4; i++) {
        M_CheckNear(output[i * 2], M_Left(i));
        M_CheckNear(output[i * 2 + 1], M_Left(i));
    }
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(pcm_that_is_not_whole_frames_fails_and_keeps_the_reader)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    READER reader = M_PcmReader(4, 2, 2);
    AUDIO_STREAM *stream = nullptr;
    const AUDIO_PCM_FORMAT format = { .sample_rate = 48000, .channels = 2 };
    CHECK(!Result_Ignore(
        AudioStream_LoadPcm(&reader, format, &options, &stream)));
    CHECK(stream == nullptr);
    CHECK_EQ_INT(Reader_Size(&reader), 18);
    const AUDIO_PCM_FORMAT no_rate = { .sample_rate = 0, .channels = 2 };
    M_CheckResult(Reader_Narrow(&reader, 2, 16));
    CHECK(!Result_Ignore(
        AudioStream_LoadPcm(&reader, no_rate, &options, &stream)));
    const AUDIO_PCM_FORMAT surround = { .sample_rate = 48000, .channels = 4 };
    CHECK(!Result_Ignore(
        AudioStream_LoadPcm(&reader, surround, &options, &stream)));
    CHECK(stream == nullptr);
    Reader_Close(&reader);
    Audio_Shutdown();
}

TEST(pcm_stream_rejects_a_slice)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    READER reader = M_PcmReader(4, 2, 0);
    READER slice = Reader_Slice(&reader, 8);
    AUDIO_STREAM *stream = nullptr;
    const AUDIO_PCM_FORMAT format = { .sample_rate = 48000, .channels = 2 };
    CHECK(
        !Result_Ignore(AudioStream_LoadPcm(&slice, format, &options, &stream)));
    CHECK(stream == nullptr);
    Reader_Close(&reader);
    Audio_Shutdown();
}

static void M_CheckStreamLength(const bool raw)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_STREAM *const stream = M_LoadSineStream(44100, 1000.0, &options, raw);
    AUDIO_INFO info;
    M_CheckResult(AudioStream_GetInfo(stream, &info));
    CHECK_EQ_INT(info.sample_rate, 44100);
    CHECK_EQ_INT(info.length, 44100);
    M_CheckResult(AudioStream_Play(stream));
    const uint32_t capacity = 48100;
    float *const output = Mem_AllocArr(capacity, sizeof(*output));
    const uint32_t count = M_MixToEnd(stream, output, capacity);
    CHECK(count >= 47998 && count <= 48002);
    CHECK_EQ_INT(AudioStream_GetPosition(stream), 44100);
    Mem_Free(output);
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(stream_at_44100_hz_resamples_to_the_output_length)
{
    M_CheckStreamLength(false);
}

TEST(pcm_stream_at_44100_hz_resamples_to_the_output_length)
{
    M_CheckStreamLength(true);
}

// Catches a linear resampler, which leaves about 0.7 of the amplitude of
// 15 kHz.
static void M_CheckStreamFrequencies(const bool raw)
{
    M_CheckResult(Audio_Init());
    const double frequencies[] = { 1000.0, 15000.0 };
    for (size_t i = 0; i < 2; i++) {
        AUDIO_PLAY_OPTIONS options = M_Options();
        AUDIO_STREAM *const stream =
            M_LoadSineStream(44100, frequencies[i], &options, raw);
        M_CheckResult(AudioStream_Play(stream));
        const uint32_t capacity = 48100;
        float *const output = Mem_AllocArr(capacity, sizeof(*output));
        const uint32_t count = M_MixToEnd(stream, output, capacity);
        CHECK(count >= 46000);
        if (count >= 46000) {
            M_CheckSine(output, 2000, 46000, frequencies[i]);
        }
        Mem_Free(output);
        AudioStream_Free(stream);
    }
    Audio_Shutdown();
}

TEST(stream_at_44100_hz_keeps_its_high_frequencies)
{
    M_CheckStreamFrequencies(false);
}

TEST(pcm_stream_at_44100_hz_keeps_its_high_frequencies)
{
    M_CheckStreamFrequencies(true);
}

// Loops whole periods of 441 Hz, 100 source frames, so that the signal
// stays continuous at the loop seam.
static void M_CheckStreamLoop(const bool raw)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    options.loop = (AUDIO_LOOP) { .enabled = true, .begin = 1000, .end = 3000 };
    AUDIO_STREAM *const stream = M_LoadSineStream(8192, 441.0, &options, raw);
    M_CheckResult(AudioStream_Play(stream));
    const uint32_t frames = 12000;
    float *const output = Mem_AllocArr(frames, sizeof(*output));
    M_MixPlayed(stream, output, frames);
    // Bounds the step between two output frames by the steepest slope of
    // the sine, with room for rounding to 16 bits.
    const double slope = 0.4 * TAU * 441.0 / 48000.0 * 1.05;
    double steepest = 0.0;
    for (uint32_t i = 100; i < frames; i++) {
        steepest = fmax(steepest, fabs(output[i] - output[i - 1]));
    }
    if (steepest > slope) {
        TEST_FAIL(
            "a step of %.4f exceeds the sine slope %.4f", steepest, slope);
    }
    M_CheckSine(output, 100, frames, 441.0);
    const uint64_t position = AudioStream_GetPosition(stream);
    CHECK(position >= 1000 && position < 3000);
    CHECK(!AudioStream_IsFinished(stream));
    Mem_Free(output);
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(stream_at_44100_hz_loops_without_a_click)
{
    M_CheckStreamLoop(false);
}

TEST(pcm_stream_at_44100_hz_loops_without_a_click)
{
    M_CheckStreamLoop(true);
}

static void M_CheckStreamSeek(const bool raw)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_STREAM *const stream = M_LoadSineStream(44100, 1000.0, &options, raw);
    M_CheckResult(AudioStream_Seek(stream, 22050));
    M_WaitStream(stream);
    M_CheckResult(AudioStream_GetResult(stream));
    CHECK_EQ_INT(AudioStream_GetPosition(stream), 22050);
    M_CheckResult(AudioStream_Play(stream));
    float output[4800];
    M_MixPlayed(stream, output, 4800);
    // Plays 4800 output frames, 4410 source frames.
    CHECK_EQ_INT(AudioStream_GetPosition(stream), 22050 + 4410);
    M_CheckSine(output, 100, 4800, 1000.0);
    AudioStream_Free(stream);
    Audio_Shutdown();
}

TEST(stream_at_44100_hz_seeks_in_source_frames)
{
    M_CheckStreamSeek(false);
}

TEST(pcm_stream_at_44100_hz_seeks_in_source_frames)
{
    M_CheckStreamSeek(true);
}

TEST(shutdown_cancels_pending_loads_and_accepts_reinitialization)
{
    M_CheckResult(Audio_Init());
    for (int i = 0; i < 24; i++) {
        READER reader = M_WaveReader(4096);
        AUDIO_SAMPLE *sample = nullptr;
        M_CheckResult(AudioSample_Load(&reader, &sample));
        if (i % 2 == 0) {
            AudioSample_Free(sample);
        }
    }
    Audio_Shutdown();
    M_CheckResult(Audio_Init());
    AUDIO_SAMPLE *const sample = M_LoadSample(8);
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_PLAYBACK *const playback = M_Play(sample, &options);
    float output[2];
    M_Mix(output, 1);
    M_CheckNear(output[0], M_Left(0));
    AudioPlayback_Free(playback);
    AudioSample_Free(sample);
    Audio_Shutdown();
}

TEST(initialized_state_follows_init_and_shutdown)
{
    CHECK(!Audio_IsInitialized());
    M_CheckResult(Audio_Init());
    CHECK(Audio_IsInitialized());
    Audio_Shutdown();
    CHECK(!Audio_IsInitialized());
}

TEST(live_bus_pause_advances_while_samples_and_streams_remain_paused)
{
    M_CheckResult(Audio_Init());
    AUDIO_PLAY_OPTIONS options = M_Options();
    options.group = AUDIO_GROUP_MUSIC;
    AUDIO_SAMPLE *const sample = M_LoadSample(8);
    AUDIO_PLAYBACK *const playback = M_Play(sample, &options);
    AUDIO_STREAM *const stream = M_LoadStream(8, &options);
    M_CheckResult(AudioStream_Play(stream));
    const AUDIO_GROUP group = AUDIO_GROUP_MUSIC;
    AUDIO_LIVE *input = nullptr;
    M_CheckResult(AudioLive_Create(&group, 1, 4, &input));
    const float pcm[] = { 0.125f, -0.125f, 0.25f, -0.25f,
                          0.375f, -0.375f, 0.5f,  -0.5f };
    const float *const buses[] = { pcm };
    M_CheckResult(AudioLive_Submit(input, buses, 4));
    Audio_SetGroupPaused(group, true);
    float output[4];
    M_Mix(output, 2);
    for (int i = 0; i < 4; i++) {
        M_CheckNear(output[i], 0.0f);
    }
    CHECK_EQ_INT(AudioStream_GetPosition(stream), 0);
    CHECK(!AudioPlayback_IsFinished(playback));
    Audio_SetGroupPaused(group, false);
    M_Mix(output, 1);
    M_CheckNear(output[0], M_Left(0) * 2 + 0.375f);
    M_CheckNear(output[1], M_Right(0) * 2 - 0.375f);
    CHECK_EQ_INT(AudioStream_GetPosition(stream), 1);
    M_Mix(output, 1);
    M_CheckNear(output[0], M_Left(1) * 2 + 0.5f);
    M_CheckNear(output[1], M_Right(1) * 2 - 0.5f);
    AudioLive_Free(input);
    AudioPlayback_Free(playback);
    AudioSample_Free(sample);
    AudioStream_Free(stream);
    Audio_Shutdown();
}
