#include "core/audio.h"

#include "core/io/writer.h"

#include "test.h"

#include <SDL3/SDL.h>

#define WAIT_MILLISECONDS 5000

static void M_CheckResult(RESULT result)
{
    CHECK(Result_Log(result, LOG_LEVEL_ERROR));
}

static READER M_Wave(void)
{
    const uint32_t frames = 4096;
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Writer_WriteBytes(&writer, "RIFF", 4);
    Writer_WriteU32(&writer, 36 + frames * 4);
    Writer_WriteBytes(&writer, "WAVEfmt ", 8);
    Writer_WriteU32(&writer, 16);
    Writer_WriteU16(&writer, 1);
    Writer_WriteU16(&writer, 2);
    Writer_WriteU32(&writer, 48000);
    Writer_WriteU32(&writer, 48000 * 4);
    Writer_WriteU16(&writer, 4);
    Writer_WriteU16(&writer, 16);
    Writer_WriteBytes(&writer, "data", 4);
    Writer_WriteU32(&writer, frames * 4);
    for (uint32_t i = 0; i < frames; i++) {
        Writer_WriteI16(&writer, 1024);
        Writer_WriteI16(&writer, -1024);
    }
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    M_CheckResult(Writer_Close(&writer));
    READER reader;
    Reader_OpenOwnedBuffer(&reader, data, size, &g_HeapAllocator, nullptr);
    return reader;
}

static AUDIO_PLAY_OPTIONS M_Options(void)
{
    return (AUDIO_PLAY_OPTIONS) {
        .group = AUDIO_GROUP_EFFECTS,
        .volume = 1.0f,
        .pitch = 1.0f,
    };
}

TEST(sdl_device_plays_a_sample_to_completion)
{
    M_CheckResult(Audio_Init());
    READER reader = M_Wave();
    AUDIO_SAMPLE *sample = nullptr;
    M_CheckResult(AudioSample_Load(&reader, &sample));
    Uint64 deadline = SDL_GetTicks() + WAIT_MILLISECONDS;
    while (AudioSample_GetState(sample) == AUDIO_LOADING
           && SDL_GetTicks() < deadline) {
        SDL_Delay(1);
    }
    M_CheckResult(AudioSample_GetResult(sample));
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_PLAYBACK *playback = nullptr;
    M_CheckResult(AudioSample_Play(sample, &options, &playback));
    AudioSample_Free(sample);
    deadline = SDL_GetTicks() + WAIT_MILLISECONDS;
    while (!AudioPlayback_IsFinished(playback) && SDL_GetTicks() < deadline) {
        SDL_Delay(1);
    }
    CHECK(AudioPlayback_IsFinished(playback));
    AudioPlayback_Free(playback);
    Audio_Shutdown();
}

TEST(sdl_device_plays_a_stream_to_completion)
{
    M_CheckResult(Audio_Init());
    READER reader = M_Wave();
    AUDIO_PLAY_OPTIONS options = M_Options();
    AUDIO_STREAM *stream = nullptr;
    M_CheckResult(AudioStream_Load(&reader, &options, &stream));
    Uint64 deadline = SDL_GetTicks() + WAIT_MILLISECONDS;
    while (AudioStream_GetState(stream) == AUDIO_LOADING
           && SDL_GetTicks() < deadline) {
        SDL_Delay(1);
    }
    M_CheckResult(AudioStream_GetResult(stream));
    M_CheckResult(AudioStream_Play(stream));
    deadline = SDL_GetTicks() + WAIT_MILLISECONDS;
    while (!AudioStream_IsFinished(stream) && SDL_GetTicks() < deadline) {
        SDL_Delay(1);
    }
    CHECK(AudioStream_IsFinished(stream));
    CHECK_EQ_INT(AudioStream_GetPosition(stream), 4096);
    AudioStream_Free(stream);
    Audio_Shutdown();
}
