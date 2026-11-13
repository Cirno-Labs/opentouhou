#include "core/audio/backend.h"

#include "core/base/utils.h"

#include <SDL3/SDL.h>

#define MIX_FRAMES 1024

static SDL_AudioStream *m_Stream;
static bool m_AudioInitialized;

static void M_Output(
    void *const user, SDL_AudioStream *const stream, const int additional,
    const int total)
{
    float buffer[MIX_FRAMES * 2];
    int remaining = additional / (int)(2 * sizeof(float));
    while (remaining > 0) {
        const uint32_t frames = (uint32_t)MIN(remaining, MIX_FRAMES);
        AudioBackend_Mix(buffer, frames);
        if (!SDL_PutAudioStreamData(
                stream, buffer, (int)(frames * 2 * sizeof(float)))) {
            break;
        }
        remaining -= (int)frames;
    }
}

RESULT AudioBackend_Init(void)
{
    FAIL_IF(
        !SDL_InitSubSystem(SDL_INIT_AUDIO), "Cannot initialize audio: %s",
        SDL_GetError());
    m_AudioInitialized = true;
    const SDL_AudioSpec format = {
        .format = SDL_AUDIO_F32,
        .channels = 2,
        .freq = AUDIO_OUTPUT_RATE,
    };
    m_Stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &format, M_Output, nullptr);
    if (m_Stream == nullptr) {
        const RESULT result =
            FAIL("Cannot open audio output: %s", SDL_GetError());
        AudioBackend_Shutdown();
        return result;
    }
    if (!SDL_ResumeAudioStreamDevice(m_Stream)) {
        const RESULT result =
            FAIL("Cannot start audio output: %s", SDL_GetError());
        AudioBackend_Shutdown();
        return result;
    }
    return OK;
}

void AudioBackend_Shutdown(void)
{
    if (m_Stream != nullptr) {
        SDL_DestroyAudioStream(m_Stream);
        m_Stream = nullptr;
    }
    if (m_AudioInitialized) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        m_AudioInitialized = false;
    }
}
