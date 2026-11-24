#include "core/audio/backend.h"

#include "core/base/memory.h"
#include "core/base/utils.h"

#include <SDL3/SDL.h>
#include <emscripten.h>
#include <stdatomic.h>
#include <string.h>

#define M_CAPACITY 32768u
#define M_MIX_FRAMES 1024u

typedef struct {
    atomic_uint read;
    atomic_uint write;
} M_OUTPUT;

static_assert(sizeof(M_OUTPUT) == 2 * sizeof(uint32_t));

static M_OUTPUT *m_Output;
static float *m_Pcm;
static SDL_Thread *m_Thread;
static atomic_bool m_Stop;
static uint32_t m_TargetFrames;

EM_JS(uint32_t, M_TargetFrames, (), {
    const audio = Module['opentouhouAudio'];
    return audio ? audio.targetFrames : 0;
})

EM_JS(bool, M_StartOutput, (void *state, float *pcm, uint32_t capacity), {
    return Module['opentouhouAudio'].start(HEAPU8.buffer, state, pcm, capacity);
})

EM_JS(void, M_StopOutput, (void *state, float *pcm), {
    Module['opentouhouAudio'].release(Module, state, pcm);
})

static int M_Mix(void *const user)
{
    float buffer[M_MIX_FRAMES * 2];
    while (!atomic_load(&m_Stop)) {
        const uint32_t read =
            atomic_load_explicit(&m_Output->read, memory_order_acquire);
        const uint32_t write =
            atomic_load_explicit(&m_Output->write, memory_order_relaxed);
        const uint32_t queued = write - read;
        if (queued >= m_TargetFrames) {
            SDL_Delay(1);
            continue;
        }
        const uint32_t count = MIN(m_TargetFrames - queued, M_MIX_FRAMES);
        if (!AudioBackend_Mix(buffer, count)) {
            SDL_Delay(1);
            continue;
        }
        const uint32_t at = write % M_CAPACITY;
        const uint32_t first = MIN(count, M_CAPACITY - at);
        memcpy(m_Pcm + at * 2, buffer, first * 2 * sizeof(float));
        memcpy(m_Pcm, buffer + first * 2, (count - first) * 2 * sizeof(float));
        atomic_store_explicit(
            &m_Output->write, write + count, memory_order_release);
    }
    return 0;
}

RESULT AudioBackend_Init(void)
{
    m_TargetFrames = M_TargetFrames();
    FAIL_IF(
        m_TargetFrames == 0 || m_TargetFrames > M_CAPACITY,
        "Browser audio output is not prepared");
    m_Output = Mem_AllocZ(sizeof(*m_Output));
    atomic_init(&m_Output->read, 0);
    atomic_init(&m_Output->write, 0);
    m_Pcm = Mem_AllocArr(M_CAPACITY, 2 * sizeof(float));
    atomic_store(&m_Stop, false);
    m_Thread = SDL_CreateThread(M_Mix, "audio output", nullptr);
    if (m_Thread == nullptr) {
        Mem_Free(m_Output);
        Mem_Free(m_Pcm);
        m_Output = nullptr;
        m_Pcm = nullptr;
        return FAIL("Cannot start audio output: %s", SDL_GetError());
    }
    if (!M_StartOutput(m_Output, m_Pcm, M_CAPACITY)) {
        AudioBackend_Shutdown();
        return FAIL("Cannot start browser audio output");
    }
    return OK;
}

void AudioBackend_Shutdown(void)
{
    if (m_Thread != nullptr) {
        atomic_store(&m_Stop, true);
        SDL_WaitThread(m_Thread, nullptr);
        m_Thread = nullptr;
    }
    if (m_Output != nullptr) {
        M_StopOutput(m_Output, m_Pcm);
        m_Output = nullptr;
        m_Pcm = nullptr;
    }
}

EMSCRIPTEN_KEEPALIVE void AudioWeb_Release(void *const state, void *const pcm)
{
    Mem_Free(state);
    Mem_Free(pcm);
}
