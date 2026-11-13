// SPDX-License-Identifier: GPL-3.0-only

#include "core/audio.h"

#include "core/audio/backend.h"
#include "core/audio/decoder.h"
#include "core/base/memory.h"
#include "core/base/string.h"
#include "core/base/utils.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdatomic.h>
#include <string.h>

#define STREAM_CAPACITY 32768
#define STREAM_PREFILL 4096
#define WORK_FRAMES 1024
#define GROUP_COUNT 3

typedef struct {
    READER reader;
    char *path;
    AUDIO_DECODER *decoder;
    const float *frames;
    uint32_t count;
    uint32_t offset;
    uint64_t position;
    bool opened;
    bool eof;
} AUDIO_JOB;

typedef struct {
    float volume;
    bool muted;
    bool paused;
} AUDIO_GROUP_STATE;

struct AUDIO_SAMPLE {
    AUDIO_SAMPLE *next;
    AUDIO_JOB job;
    AUDIO_LOAD_STATE state;
    AUDIO_INFO info;
    char *error;
    float *pcm;
    size_t count;
    size_t capacity;
    size_t references;
    atomic_bool *release_signal;
};

struct AUDIO_PLAYBACK {
    AUDIO_PLAYBACK *next;
    AUDIO_SAMPLE *sample;
    AUDIO_PLAY_OPTIONS options;
    double position;
    bool paused;
    bool finished;
};

struct AUDIO_STREAM {
    AUDIO_STREAM *next;
    AUDIO_JOB job;
    AUDIO_LOAD_STATE state;
    AUDIO_INFO info;
    char *error;
    AUDIO_PLAY_OPTIONS options;
    float *pcm;
    uint64_t *positions;
    uint64_t read;
    uint64_t write;
    double phase;
    uint64_t position;
    uint64_t requested_position;
    uint64_t generation;
    uint64_t worker_generation;
    uint64_t configuration;
    bool paused;
    bool finished;
    bool disposed;
    atomic_bool *release_signal;
    bool eof;
    float fade_start;
    float fade_end;
    uint64_t fade_frames;
    uint64_t fade_position;
    bool fade_stop;
};

static SDL_Mutex *m_Mutex;
static SDL_Thread *m_Worker;
static atomic_bool m_Stop;
static AUDIO_SAMPLE *m_Samples;
static AUDIO_PLAYBACK *m_Playbacks;
static AUDIO_STREAM *m_Streams;
static AUDIO_GROUP_STATE m_Groups[GROUP_COUNT];
static float m_Volume;
static bool m_Muted;

static bool M_ValidGroup(const AUDIO_GROUP group)
{
    return group >= AUDIO_GROUP_MUSIC && group <= AUDIO_GROUP_INTERFACE;
}

static RESULT M_CheckVolume(const float volume)
{
    FAIL_IF(!isfinite(volume) || volume < 0.0f, "Invalid audio volume");
    return OK;
}

static RESULT M_CheckPan(const float pan)
{
    FAIL_IF(!isfinite(pan) || pan < -1.0f || pan > 1.0f, "Invalid audio pan");
    return OK;
}

static RESULT M_CheckPitch(const float pitch)
{
    FAIL_IF(!isfinite(pitch) || pitch <= 0.0f, "Invalid audio pitch");
    return OK;
}

static RESULT M_CheckLoop(const AUDIO_LOOP loop, const AUDIO_INFO *const info)
{
    FAIL_IF(loop.enabled && loop.begin >= loop.end, "Invalid audio loop range");
    FAIL_IF(
        loop.enabled && info != nullptr && info->length_known
            && loop.end > info->length,
        "Audio loop exceeds the source length");
    return OK;
}

static RESULT M_CheckStreamPitch(
    const float pitch, const AUDIO_INFO *const info)
{
    MUST(M_CheckPitch(pitch));
    FAIL_IF(
        info->sample_rate != 0
            && (double)pitch * info->sample_rate / AUDIO_OUTPUT_RATE
                > STREAM_CAPACITY - 2,
        "Audio stream playback rate exceeds the buffer capacity");
    return OK;
}

static RESULT M_CheckOptions(const AUDIO_PLAY_OPTIONS *const options)
{
    FAIL_IF(options == nullptr, "Missing audio playback options");
    FAIL_IF(!M_ValidGroup(options->group), "Invalid audio group");
    MUST(M_CheckVolume(options->volume));
    MUST(M_CheckPan(options->pan));
    MUST(M_CheckPitch(options->pitch));
    return M_CheckLoop(options->loop, nullptr);
}

static RESULT M_CheckReader(const READER *const reader)
{
    FAIL_IF(reader == nullptr, "Missing audio reader");
    FAIL_IF(
        !reader->owns_file && reader->buffer_allocator == nullptr,
        "Audio requires an independently owned reader");
    return Reader_Result(reader);
}

static RESULT M_CheckInitialized(void)
{
    FAIL_IF(m_Mutex == nullptr, "Audio is not initialized");
    return OK;
}

static void M_CloseJob(AUDIO_JOB *const job)
{
    AudioDecoder_Free(job->decoder);
    job->decoder = nullptr;
    Reader_Close(&job->reader);
    Mem_Free(job->path);
    job->path = nullptr;
    job->frames = nullptr;
    job->count = 0;
    job->offset = 0;
}

static RESULT M_OpenJob(AUDIO_JOB *const job)
{
    if (job->opened) {
        return OK;
    }
    if (job->path != nullptr) {
        MUST(Reader_OpenFile(&job->reader, job->path));
    }
    MUST(AudioDecoder_Open(&job->reader, &job->decoder));
    job->opened = true;
    return OK;
}

static RESULT M_ReadJob(AUDIO_JOB *const job)
{
    if (job->offset < job->count || job->eof) {
        return OK;
    }
    job->offset = 0;
    job->count = 0;
    MUST(AudioDecoder_Read(job->decoder, &job->frames, &job->count));
    job->eof = job->count == 0;
    return OK;
}

static RESULT M_SeekJob(AUDIO_JOB *const job, const uint64_t position)
{
    MUST(AudioDecoder_Seek(job->decoder, position));
    job->position = position;
    job->offset = 0;
    job->count = 0;
    job->eof = false;
    return OK;
}

static void M_FreeSample(AUDIO_SAMPLE *const sample)
{
    atomic_bool *const released = sample->release_signal;
    M_CloseJob(&sample->job);
    Mem_Free(sample->error);
    Mem_Free(sample->pcm);
    Mem_Free(sample);
    if (released != nullptr) {
        atomic_store(released, true);
    }
}

static void M_FreeStream(AUDIO_STREAM *const stream)
{
    atomic_bool *const released = stream->release_signal;
    M_CloseJob(&stream->job);
    Mem_Free(stream->error);
    Mem_Free(stream->pcm);
    Mem_Free(stream->positions);
    Mem_Free(stream);
    if (released != nullptr) {
        atomic_store(released, true);
    }
}

static void M_SampleFailure(AUDIO_SAMPLE *const sample, const RESULT result)
{
    SDL_LockMutex(m_Mutex);
    sample->error = Str_Dup(result.message);
    sample->state = AUDIO_FAILED;
    SDL_UnlockMutex(m_Mutex);
    Result_Ignore(result);
    M_CloseJob(&sample->job);
}

static void M_StreamFailure(
    AUDIO_STREAM *const stream, const uint64_t generation,
    const uint64_t configuration, const RESULT result)
{
    SDL_LockMutex(m_Mutex);
    if (stream->generation == generation
        && stream->configuration == configuration && !stream->disposed) {
        Mem_Free(stream->error);
        stream->error = Str_Dup(result.message);
        stream->state = AUDIO_FAILED;
        stream->paused = true;
        stream->finished = true;
    }
    SDL_UnlockMutex(m_Mutex);
    Result_Ignore(result);
}

static bool M_PumpSample(AUDIO_SAMPLE *const sample)
{
    RESULT result = M_OpenJob(&sample->job);
    if (!IS_OK(result)) {
        M_SampleFailure(sample, result);
        return true;
    }
    result = M_ReadJob(&sample->job);
    if (!IS_OK(result)) {
        M_SampleFailure(sample, result);
        return true;
    }
    AUDIO_JOB *const job = &sample->job;
    if (job->eof) {
        AUDIO_INFO info = AudioDecoder_GetInfo(job->decoder);
        info.length = sample->count;
        info.length_known = true;
        M_CloseJob(job);
        SDL_LockMutex(m_Mutex);
        sample->info = info;
        sample->state = AUDIO_READY;
        SDL_UnlockMutex(m_Mutex);
        return true;
    }
    const uint32_t count = MIN(job->count - job->offset, WORK_FRAMES);
    if (sample->count > SIZE_MAX / (2 * sizeof(float)) - count) {
        M_SampleFailure(sample, FAIL("Decoded audio sample is too large"));
        return true;
    }
    if (sample->count + count > sample->capacity) {
        const size_t required = sample->count + count;
        const size_t maximum = SIZE_MAX / (2 * sizeof(float));
        sample->capacity = sample->capacity > maximum / 2
            ? required
            : MAX(required, MAX((size_t)4096, sample->capacity * 2));
        sample->pcm =
            Mem_ReallocArr(sample->pcm, sample->capacity, 2 * sizeof(float));
    }
    memcpy(
        sample->pcm + sample->count * 2, job->frames + job->offset * 2,
        count * 2 * sizeof(float));
    sample->count += count;
    job->offset += count;
    return true;
}

static bool M_PumpStream(AUDIO_STREAM *const stream)
{
    SDL_LockMutex(m_Mutex);
    const uint64_t generation = stream->generation;
    const uint64_t configuration = stream->configuration;
    const uint64_t desired = stream->requested_position;
    const AUDIO_LOOP loop = stream->options.loop;
    const uint64_t available = stream->write - stream->read;
    const bool needs_seek = stream->worker_generation != generation;
    const bool full = available == STREAM_CAPACITY;
    SDL_UnlockMutex(m_Mutex);
    if (full && !needs_seek) {
        return false;
    }
    AUDIO_JOB *const job = &stream->job;
    RESULT result = M_OpenJob(job);
    if (!IS_OK(result)) {
        M_StreamFailure(stream, generation, configuration, result);
        return true;
    }
    const AUDIO_INFO info = AudioDecoder_GetInfo(job->decoder);
    result = M_CheckLoop(loop, &info);
    if (IS_OK(result)) {
        SDL_LockMutex(m_Mutex);
        const float pitch = stream->options.pitch;
        SDL_UnlockMutex(m_Mutex);
        result = M_CheckStreamPitch(pitch, &info);
    }
    if (!IS_OK(result)) {
        M_StreamFailure(stream, generation, configuration, result);
        return true;
    }
    if (needs_seek) {
        result = M_SeekJob(job, desired);
        if (!IS_OK(result)) {
            M_StreamFailure(stream, generation, configuration, result);
            return true;
        }
        SDL_LockMutex(m_Mutex);
        stream->worker_generation = generation;
        if (stream->generation == generation) {
            stream->read = 0;
            stream->write = 0;
            stream->phase = 0.0;
            stream->position = desired;
            stream->eof = false;
            stream->info = info;
        }
        SDL_UnlockMutex(m_Mutex);
    }
    if (loop.enabled && job->eof && job->position < loop.end) {
        M_StreamFailure(
            stream, generation, configuration,
            FAIL("Audio loop exceeds the source length"));
        return true;
    }
    if (loop.enabled && (job->position >= loop.end || job->eof)) {
        result = M_SeekJob(job, loop.begin);
        if (!IS_OK(result)) {
            M_StreamFailure(stream, generation, configuration, result);
            return true;
        }
    }
    result = M_ReadJob(job);
    if (!IS_OK(result)) {
        M_StreamFailure(stream, generation, configuration, result);
        return true;
    }
    if (job->eof && loop.enabled) {
        M_StreamFailure(
            stream, generation, configuration,
            FAIL("Audio loop reaches an empty source range"));
        return true;
    }
    SDL_LockMutex(m_Mutex);
    if (stream->generation != generation || stream->disposed) {
        SDL_UnlockMutex(m_Mutex);
        return true;
    }
    const uint64_t space = STREAM_CAPACITY - (stream->write - stream->read);
    uint32_t count = (uint32_t)MIN(
        (uint64_t)MIN(job->count - job->offset, WORK_FRAMES), space);
    if (loop.enabled) {
        count = (uint32_t)MIN((uint64_t)count, loop.end - job->position);
    }
    for (uint32_t i = 0; i < count; i++) {
        const size_t at = (size_t)((stream->write + i) % STREAM_CAPACITY);
        stream->pcm[at * 2] = job->frames[(job->offset + i) * 2];
        stream->pcm[at * 2 + 1] = job->frames[(job->offset + i) * 2 + 1];
        stream->positions[at] = job->position + i;
    }
    stream->write += count;
    job->offset += count;
    job->position += count;
    stream->eof = job->eof;
    if (stream->state == AUDIO_LOADING
        && (stream->write - stream->read >= STREAM_PREFILL || stream->eof)) {
        stream->state = AUDIO_READY;
    }
    SDL_UnlockMutex(m_Mutex);
    return count != 0 || needs_seek;
}

static int M_Worker(void *const user)
{
    while (!atomic_load(&m_Stop)) {
        bool work = false;
        SDL_LockMutex(m_Mutex);
        AUDIO_SAMPLE **sample_link = &m_Samples;
        while (*sample_link != nullptr) {
            AUDIO_SAMPLE *const sample = *sample_link;
            if (sample->references == 0) {
                *sample_link = sample->next;
                SDL_UnlockMutex(m_Mutex);
                M_FreeSample(sample);
                SDL_LockMutex(m_Mutex);
                work = true;
            } else {
                sample_link = &sample->next;
                if (sample->state == AUDIO_LOADING) {
                    SDL_UnlockMutex(m_Mutex);
                    work = M_PumpSample(sample) || work;
                    SDL_LockMutex(m_Mutex);
                }
            }
        }
        AUDIO_STREAM **stream_link = &m_Streams;
        while (*stream_link != nullptr) {
            AUDIO_STREAM *const stream = *stream_link;
            if (stream->disposed) {
                *stream_link = stream->next;
                SDL_UnlockMutex(m_Mutex);
                M_FreeStream(stream);
                SDL_LockMutex(m_Mutex);
                work = true;
            } else {
                stream_link = &stream->next;
                if (stream->state != AUDIO_FAILED) {
                    SDL_UnlockMutex(m_Mutex);
                    work = M_PumpStream(stream) || work;
                    SDL_LockMutex(m_Mutex);
                }
            }
        }
        SDL_UnlockMutex(m_Mutex);
        if (!work) {
            SDL_Delay(1);
        }
    }
    return 0;
}

static RESULT M_LoadResult(
    const AUDIO_LOAD_STATE state, const char *const error)
{
    FAIL_IF(state == AUDIO_LOADING, "Audio loading is incomplete");
    FAIL_IF(state == AUDIO_FAILED, "%s", error);
    return OK;
}

static float M_Gain(const AUDIO_PLAY_OPTIONS *const options, const int channel)
{
    const AUDIO_GROUP_STATE *const group = &m_Groups[options->group];
    if (m_Muted || group->muted) {
        return 0.0f;
    }
    const float balance = channel == 0 ? MIN(1.0f, 1.0f - options->pan)
                                       : MIN(1.0f, 1.0f + options->pan);
    return m_Volume * group->volume * options->volume * balance;
}

static void M_MixPlayback(
    AUDIO_PLAYBACK *const playback, float *const out, const uint32_t frames)
{
    if (playback->paused || playback->finished
        || m_Groups[playback->options.group].paused) {
        return;
    }
    const AUDIO_SAMPLE *const sample = playback->sample;
    const AUDIO_LOOP loop = playback->options.loop;
    const double limit =
        loop.enabled ? (double)loop.end : (double)sample->count;
    const double step = (double)sample->info.sample_rate / AUDIO_OUTPUT_RATE
        * playback->options.pitch;
    const float left = M_Gain(&playback->options, 0);
    const float right = M_Gain(&playback->options, 1);
    for (uint32_t i = 0; i < frames; i++) {
        if (playback->position >= limit) {
            if (!loop.enabled) {
                playback->finished = true;
                break;
            }
            playback->position = loop.begin
                + fmod(playback->position - loop.begin,
                       (double)(loop.end - loop.begin));
        }
        const size_t at = (size_t)playback->position;
        size_t next = at + 1;
        if ((double)next >= limit) {
            next = loop.enabled ? (size_t)loop.begin : at;
        }
        const float fraction = (float)(playback->position - (double)at);
        out[i * 2] +=
            (sample->pcm[at * 2]
             + (sample->pcm[next * 2] - sample->pcm[at * 2]) * fraction)
            * left;
        out[i * 2 + 1] +=
            (sample->pcm[at * 2 + 1]
             + (sample->pcm[next * 2 + 1] - sample->pcm[at * 2 + 1]) * fraction)
            * right;
        playback->position += step;
    }
    if (!loop.enabled && playback->position >= limit) {
        playback->finished = true;
    }
}

static void M_MixStream(
    AUDIO_STREAM *const stream, float *const out, const uint32_t frames)
{
    if (stream->disposed || stream->paused || stream->finished
        || stream->state != AUDIO_READY
        || m_Groups[stream->options.group].paused) {
        return;
    }
    const double step = (double)stream->info.sample_rate / AUDIO_OUTPUT_RATE
        * stream->options.pitch;
    for (uint32_t i = 0; i < frames; i++) {
        const uint64_t available = stream->write - stream->read;
        const double advance = stream->phase + step;
        if (available == 0 && stream->eof) {
            stream->finished = true;
            break;
        }
        if (available == 0 || (!stream->eof && advance > (double)available)
            || (!stream->eof && stream->phase != 0.0 && available < 2)) {
            break;
        }
        const size_t at = (size_t)(stream->read % STREAM_CAPACITY);
        const size_t next = available > 1 ? (at + 1) % STREAM_CAPACITY : at;
        const float fraction = (float)stream->phase;
        out[i * 2] +=
            (stream->pcm[at * 2]
             + (stream->pcm[next * 2] - stream->pcm[at * 2]) * fraction)
            * M_Gain(&stream->options, 0);
        out[i * 2 + 1] +=
            (stream->pcm[at * 2 + 1]
             + (stream->pcm[next * 2 + 1] - stream->pcm[at * 2 + 1]) * fraction)
            * M_Gain(&stream->options, 1);
        const uint64_t consumed =
            (uint64_t)MIN(floor(advance), (double)available);
        stream->phase = advance - floor(advance);
        stream->read += consumed;
        if (consumed != 0) {
            stream->position = stream->read < stream->write
                ? stream->positions[stream->read % STREAM_CAPACITY]
                : stream->positions[(stream->read - 1) % STREAM_CAPACITY] + 1;
        }
        if (stream->fade_frames != 0) {
            stream->fade_position++;
            const float progress =
                (float)((double)stream->fade_position / stream->fade_frames);
            stream->options.volume = stream->fade_start
                + (stream->fade_end - stream->fade_start) * progress;
            if (stream->fade_position == stream->fade_frames) {
                stream->options.volume = stream->fade_end;
                stream->fade_frames = 0;
                if (stream->fade_stop) {
                    stream->finished = true;
                    stream->paused = true;
                    break;
                }
            }
        }
        if (stream->read == stream->write && stream->eof) {
            stream->finished = true;
            break;
        }
    }
}

static AUDIO_STREAM *M_CreateStream(const AUDIO_PLAY_OPTIONS *const options)
{
    AUDIO_STREAM *const stream = Mem_AllocZ(sizeof(*stream));
    stream->pcm = Mem_AllocArr(STREAM_CAPACITY, 2 * sizeof(float));
    stream->positions = Mem_AllocArr(STREAM_CAPACITY, sizeof(uint64_t));
    stream->options = *options;
    stream->paused = true;
    stream->generation = 1;
    return stream;
}

static void M_AddStream(AUDIO_STREAM *const stream)
{
    SDL_LockMutex(m_Mutex);
    stream->next = m_Streams;
    m_Streams = stream;
    SDL_UnlockMutex(m_Mutex);
}

RESULT Audio_Init(void)
{
    FAIL_IF(m_Mutex != nullptr, "Audio is already initialized");
    m_Mutex = SDL_CreateMutex();
    FAIL_IF(
        m_Mutex == nullptr, "Cannot create audio mutex: %s", SDL_GetError());
    atomic_store(&m_Stop, false);
    m_Volume = 1.0f;
    m_Muted = false;
    for (int i = 0; i < GROUP_COUNT; i++) {
        m_Groups[i] = (AUDIO_GROUP_STATE) { .volume = 1.0f };
    }
    m_Worker = SDL_CreateThread(M_Worker, "audio decoder", nullptr);
    if (m_Worker == nullptr) {
        SDL_DestroyMutex(m_Mutex);
        m_Mutex = nullptr;
        return FAIL("Cannot start audio decoder: %s", SDL_GetError());
    }
    const RESULT result = AudioBackend_Init();
    if (!IS_OK(result)) {
        Audio_Shutdown();
    }
    return result;
}

void Audio_Shutdown(void)
{
    if (m_Mutex == nullptr) {
        return;
    }
    AudioBackend_Shutdown();
    atomic_store(&m_Stop, true);
    SDL_WaitThread(m_Worker, nullptr);
    m_Worker = nullptr;
    while (m_Playbacks != nullptr) {
        AUDIO_PLAYBACK *const playback = m_Playbacks;
        m_Playbacks = playback->next;
        Mem_Free(playback);
    }
    while (m_Samples != nullptr) {
        AUDIO_SAMPLE *const sample = m_Samples;
        m_Samples = sample->next;
        M_FreeSample(sample);
    }
    while (m_Streams != nullptr) {
        AUDIO_STREAM *const stream = m_Streams;
        m_Streams = stream->next;
        M_FreeStream(stream);
    }
    SDL_DestroyMutex(m_Mutex);
    m_Mutex = nullptr;
}

bool Audio_IsInitialized(void)
{
    return m_Mutex != nullptr;
}

bool AudioBackend_Mix(float *const out, const uint32_t frames)
{
    memset(out, 0, (size_t)frames * 2 * sizeof(float));
    if (m_Mutex == nullptr || !SDL_TryLockMutex(m_Mutex)) {
        return false;
    }
    for (AUDIO_PLAYBACK *playback = m_Playbacks; playback != nullptr;
         playback = playback->next) {
        M_MixPlayback(playback, out, frames);
    }
    for (AUDIO_STREAM *stream = m_Streams; stream != nullptr;
         stream = stream->next) {
        M_MixStream(stream, out, frames);
    }
    SDL_UnlockMutex(m_Mutex);
    return true;
}

RESULT AudioSample_LoadFile(const char *const path, AUDIO_SAMPLE **const out)
{
    *out = nullptr;
    MUST(M_CheckInitialized());
    FAIL_IF(path == nullptr || path[0] == '\0', "Missing audio path");
    AUDIO_SAMPLE *const sample = Mem_AllocZ(sizeof(*sample));
    sample->job.path = Str_Dup(path);
    sample->references = 1;
    SDL_LockMutex(m_Mutex);
    sample->next = m_Samples;
    m_Samples = sample;
    SDL_UnlockMutex(m_Mutex);
    *out = sample;
    return OK;
}

RESULT AudioSample_Load(READER *const reader, AUDIO_SAMPLE **const out)
{
    *out = nullptr;
    MUST(M_CheckInitialized());
    MUST(M_CheckReader(reader));
    AUDIO_SAMPLE *const sample = Mem_AllocZ(sizeof(*sample));
    sample->job.reader = *reader;
    *reader = (READER) {};
    sample->references = 1;
    SDL_LockMutex(m_Mutex);
    sample->next = m_Samples;
    m_Samples = sample;
    SDL_UnlockMutex(m_Mutex);
    *out = sample;
    return OK;
}

AUDIO_LOAD_STATE AudioSample_GetState(const AUDIO_SAMPLE *const sample)
{
    SDL_LockMutex(m_Mutex);
    const AUDIO_LOAD_STATE state = sample->state;
    SDL_UnlockMutex(m_Mutex);
    return state;
}

RESULT AudioSample_GetResult(const AUDIO_SAMPLE *const sample)
{
    SDL_LockMutex(m_Mutex);
    const RESULT result = M_LoadResult(sample->state, sample->error);
    SDL_UnlockMutex(m_Mutex);
    return result;
}

RESULT AudioSample_GetInfo(
    const AUDIO_SAMPLE *const sample, AUDIO_INFO *const out)
{
    SDL_LockMutex(m_Mutex);
    const RESULT result = M_LoadResult(sample->state, sample->error);
    if (IS_OK(result)) {
        *out = sample->info;
    }
    SDL_UnlockMutex(m_Mutex);
    return result;
}

void AudioSample_Free(AUDIO_SAMPLE *const sample)
{
    if (sample == nullptr) {
        return;
    }
    atomic_bool released = false;
    SDL_LockMutex(m_Mutex);
    sample->references--;
    const bool wait = sample->references == 0;
    if (wait) {
        sample->release_signal = &released;
    }
    SDL_UnlockMutex(m_Mutex);
    while (wait && !atomic_load(&released)) {
        SDL_Delay(1);
    }
}

RESULT AudioSample_Play(
    AUDIO_SAMPLE *const sample, const AUDIO_PLAY_OPTIONS *const options,
    AUDIO_PLAYBACK **const out)
{
    *out = nullptr;
    MUST(M_CheckOptions(options));
    SDL_LockMutex(m_Mutex);
    RESULT result = M_LoadResult(sample->state, sample->error);
    if (IS_OK(result)) {
        result = M_CheckLoop(options->loop, &sample->info);
    }
    if (!IS_OK(result)) {
        SDL_UnlockMutex(m_Mutex);
        return result;
    }
    AUDIO_PLAYBACK *const playback = Mem_AllocZ(sizeof(*playback));
    playback->sample = sample;
    playback->options = *options;
    playback->finished = sample->count == 0;
    playback->next = m_Playbacks;
    m_Playbacks = playback;
    sample->references++;
    SDL_UnlockMutex(m_Mutex);
    *out = playback;
    return OK;
}

bool AudioPlayback_IsFinished(const AUDIO_PLAYBACK *const playback)
{
    SDL_LockMutex(m_Mutex);
    const bool finished = playback->finished;
    SDL_UnlockMutex(m_Mutex);
    return finished;
}

void AudioPlayback_Restart(AUDIO_PLAYBACK *const playback)
{
    SDL_LockMutex(m_Mutex);
    playback->position = 0.0;
    playback->paused = false;
    playback->finished = playback->sample->count == 0;
    SDL_UnlockMutex(m_Mutex);
}

void AudioPlayback_Stop(AUDIO_PLAYBACK *const playback)
{
    SDL_LockMutex(m_Mutex);
    playback->finished = true;
    SDL_UnlockMutex(m_Mutex);
}

void AudioPlayback_SetPaused(AUDIO_PLAYBACK *const playback, const bool paused)
{
    SDL_LockMutex(m_Mutex);
    playback->paused = paused;
    SDL_UnlockMutex(m_Mutex);
}

RESULT AudioPlayback_SetVolume(
    AUDIO_PLAYBACK *const playback, const float volume)
{
    MUST(M_CheckVolume(volume));
    SDL_LockMutex(m_Mutex);
    playback->options.volume = volume;
    SDL_UnlockMutex(m_Mutex);
    return OK;
}

RESULT AudioPlayback_SetPan(AUDIO_PLAYBACK *const playback, const float pan)
{
    MUST(M_CheckPan(pan));
    SDL_LockMutex(m_Mutex);
    playback->options.pan = pan;
    SDL_UnlockMutex(m_Mutex);
    return OK;
}

RESULT AudioPlayback_SetPitch(AUDIO_PLAYBACK *const playback, const float pitch)
{
    MUST(M_CheckPitch(pitch));
    SDL_LockMutex(m_Mutex);
    playback->options.pitch = pitch;
    SDL_UnlockMutex(m_Mutex);
    return OK;
}

RESULT AudioPlayback_SetLoop(
    AUDIO_PLAYBACK *const playback, const AUDIO_LOOP loop)
{
    MUST(M_CheckLoop(loop, &playback->sample->info));
    SDL_LockMutex(m_Mutex);
    playback->options.loop = loop;
    SDL_UnlockMutex(m_Mutex);
    return OK;
}

void AudioPlayback_Free(AUDIO_PLAYBACK *const playback)
{
    if (playback == nullptr) {
        return;
    }
    SDL_LockMutex(m_Mutex);
    AUDIO_PLAYBACK **link = &m_Playbacks;
    while (*link != playback) {
        link = &(*link)->next;
    }
    *link = playback->next;
    playback->sample->references--;
    SDL_UnlockMutex(m_Mutex);
    Mem_Free(playback);
}

RESULT AudioStream_LoadFile(
    const char *const path, const AUDIO_PLAY_OPTIONS *const options,
    AUDIO_STREAM **const out)
{
    *out = nullptr;
    MUST(M_CheckInitialized());
    MUST(M_CheckOptions(options));
    FAIL_IF(path == nullptr || path[0] == '\0', "Missing audio path");
    AUDIO_STREAM *const stream = M_CreateStream(options);
    stream->job.path = Str_Dup(path);
    M_AddStream(stream);
    *out = stream;
    return OK;
}

RESULT AudioStream_Load(
    READER *const reader, const AUDIO_PLAY_OPTIONS *const options,
    AUDIO_STREAM **const out)
{
    *out = nullptr;
    MUST(M_CheckInitialized());
    MUST(M_CheckOptions(options));
    MUST(M_CheckReader(reader));
    AUDIO_STREAM *const stream = M_CreateStream(options);
    stream->job.reader = *reader;
    *reader = (READER) {};
    M_AddStream(stream);
    *out = stream;
    return OK;
}

AUDIO_LOAD_STATE AudioStream_GetState(const AUDIO_STREAM *const stream)
{
    SDL_LockMutex(m_Mutex);
    const AUDIO_LOAD_STATE state = stream->state;
    SDL_UnlockMutex(m_Mutex);
    return state;
}

RESULT AudioStream_GetResult(const AUDIO_STREAM *const stream)
{
    SDL_LockMutex(m_Mutex);
    const RESULT result = M_LoadResult(stream->state, stream->error);
    SDL_UnlockMutex(m_Mutex);
    return result;
}

RESULT AudioStream_GetInfo(
    const AUDIO_STREAM *const stream, AUDIO_INFO *const out)
{
    SDL_LockMutex(m_Mutex);
    const RESULT result = M_LoadResult(stream->state, stream->error);
    if (IS_OK(result)) {
        *out = stream->info;
    }
    SDL_UnlockMutex(m_Mutex);
    return result;
}

RESULT AudioStream_Play(AUDIO_STREAM *const stream)
{
    SDL_LockMutex(m_Mutex);
    const RESULT result = M_LoadResult(stream->state, stream->error);
    if (IS_OK(result)) {
        stream->paused = false;
        stream->finished = false;
    }
    SDL_UnlockMutex(m_Mutex);
    return result;
}

void AudioStream_SetPaused(AUDIO_STREAM *const stream, const bool paused)
{
    SDL_LockMutex(m_Mutex);
    if (stream->state == AUDIO_READY) {
        stream->paused = paused;
    }
    SDL_UnlockMutex(m_Mutex);
}

void AudioStream_Stop(AUDIO_STREAM *const stream)
{
    SDL_LockMutex(m_Mutex);
    stream->paused = true;
    stream->finished = true;
    SDL_UnlockMutex(m_Mutex);
}

bool AudioStream_IsFinished(const AUDIO_STREAM *const stream)
{
    SDL_LockMutex(m_Mutex);
    const bool finished = stream->finished;
    SDL_UnlockMutex(m_Mutex);
    return finished;
}

RESULT AudioStream_SetVolume(AUDIO_STREAM *const stream, const float volume)
{
    MUST(M_CheckVolume(volume));
    SDL_LockMutex(m_Mutex);
    stream->options.volume = volume;
    stream->fade_frames = 0;
    SDL_UnlockMutex(m_Mutex);
    return OK;
}

RESULT AudioStream_SetPan(AUDIO_STREAM *const stream, const float pan)
{
    MUST(M_CheckPan(pan));
    SDL_LockMutex(m_Mutex);
    stream->options.pan = pan;
    SDL_UnlockMutex(m_Mutex);
    return OK;
}

RESULT AudioStream_SetPitch(AUDIO_STREAM *const stream, const float pitch)
{
    MUST(M_CheckPitch(pitch));
    SDL_LockMutex(m_Mutex);
    const RESULT result = M_CheckStreamPitch(pitch, &stream->info);
    if (IS_OK(result)) {
        stream->options.pitch = pitch;
        stream->configuration++;
    }
    SDL_UnlockMutex(m_Mutex);
    return result;
}

RESULT AudioStream_SetLoop(AUDIO_STREAM *const stream, const AUDIO_LOOP loop)
{
    SDL_LockMutex(m_Mutex);
    const RESULT result = M_CheckLoop(
        loop, stream->info.sample_rate != 0 ? &stream->info : nullptr);
    if (IS_OK(result)) {
        stream->options.loop = loop;
        stream->configuration++;
    }
    SDL_UnlockMutex(m_Mutex);
    return result;
}

RESULT AudioStream_Seek(AUDIO_STREAM *const stream, const uint64_t position)
{
    SDL_LockMutex(m_Mutex);
    if (stream->state == AUDIO_FAILED
        || (stream->info.length_known && position > stream->info.length)) {
        SDL_UnlockMutex(m_Mutex);
        return FAIL("Cannot seek this audio stream to the requested position");
    }
    stream->requested_position = position;
    stream->generation++;
    stream->paused = true;
    stream->finished = false;
    stream->state = AUDIO_LOADING;
    SDL_UnlockMutex(m_Mutex);
    return OK;
}

uint64_t AudioStream_GetPosition(const AUDIO_STREAM *const stream)
{
    SDL_LockMutex(m_Mutex);
    const uint64_t position = stream->position;
    SDL_UnlockMutex(m_Mutex);
    return position;
}

RESULT AudioStream_Fade(
    AUDIO_STREAM *const stream, const float volume, const double seconds,
    const bool stop_after)
{
    MUST(M_CheckVolume(volume));
    FAIL_IF(
        !isfinite(seconds) || seconds < 0.0
            || seconds >= (double)UINT64_MAX / AUDIO_OUTPUT_RATE,
        "Invalid audio fade duration");
    SDL_LockMutex(m_Mutex);
    stream->fade_start = stream->options.volume;
    stream->fade_end = volume;
    stream->fade_frames = (uint64_t)(seconds * AUDIO_OUTPUT_RATE);
    stream->fade_position = 0;
    stream->fade_stop = stop_after;
    if (stream->fade_frames == 0) {
        stream->options.volume = volume;
        if (stop_after) {
            stream->finished = true;
            stream->paused = true;
        }
    }
    SDL_UnlockMutex(m_Mutex);
    return OK;
}

void AudioStream_Free(AUDIO_STREAM *const stream)
{
    if (stream == nullptr) {
        return;
    }
    atomic_bool released = false;
    SDL_LockMutex(m_Mutex);
    stream->release_signal = &released;
    stream->disposed = true;
    SDL_UnlockMutex(m_Mutex);
    while (!atomic_load(&released)) {
        SDL_Delay(1);
    }
}

RESULT Audio_SetMasterVolume(const float volume)
{
    MUST(M_CheckInitialized());
    MUST(M_CheckVolume(volume));
    SDL_LockMutex(m_Mutex);
    m_Volume = volume;
    SDL_UnlockMutex(m_Mutex);
    return OK;
}

void Audio_SetMuted(const bool muted)
{
    SDL_LockMutex(m_Mutex);
    m_Muted = muted;
    SDL_UnlockMutex(m_Mutex);
}

RESULT Audio_SetGroupVolume(const AUDIO_GROUP group, const float volume)
{
    MUST(M_CheckInitialized());
    MUST(M_CheckVolume(volume));
    FAIL_IF(!M_ValidGroup(group), "Invalid audio group");
    SDL_LockMutex(m_Mutex);
    m_Groups[group].volume = volume;
    SDL_UnlockMutex(m_Mutex);
    return OK;
}

void Audio_SetGroupMuted(const AUDIO_GROUP group, const bool muted)
{
    if (M_ValidGroup(group)) {
        SDL_LockMutex(m_Mutex);
        m_Groups[group].muted = muted;
        SDL_UnlockMutex(m_Mutex);
    }
}

void Audio_SetGroupPaused(const AUDIO_GROUP group, const bool paused)
{
    if (M_ValidGroup(group)) {
        SDL_LockMutex(m_Mutex);
        m_Groups[group].paused = paused;
        SDL_UnlockMutex(m_Mutex);
    }
}
