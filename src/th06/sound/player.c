#include "th06/sound/player.h"

#include "th06/files.h"
#include "th06/sound/events.h"

#include "core/audio.h"
#include "core/base/utils.h"
#include "core/math.h"

#include <math.h>
#include <string.h>

#define M_FILE_COUNT 26
#define M_FADE_DEPTH 5000
#define M_FADE_FRAMES_PER_SECOND 60.0f
// Turns a position of a .pos file into bytes, as for 16-bit stereo audio.
#define M_POS_BYTES 4

// Holds an entry of the sound table: its file in m_Files and its volume
// in hundredths of a decibel.
typedef struct {
    int32_t file;
    int16_t volume;
} M_SOUND;

typedef struct {
    uint16_t block_align;
    // Holds the size of the data chunk, cut to what the file holds.
    uint32_t data_size;
} M_WAVE;

typedef struct {
    bool enabled;
    bool effects_loaded;
    AUDIO_SAMPLE *samples[M_FILE_COUNT];
    AUDIO_PLAYBACK *playbacks[SOUND_COUNT];
    AUDIO_STREAM *music;
    bool music_starting;
    bool fading;
    int32_t fade_frames;
    int32_t fade_total;
} M_PLAYER;

static const char *const m_Files[M_FILE_COUNT] = {
    "data/wav/plst00.wav",   "data/wav/enep00.wav",   "data/wav/pldead00.wav",
    "data/wav/power0.wav",   "data/wav/power1.wav",   "data/wav/tan00.wav",
    "data/wav/tan01.wav",    "data/wav/tan02.wav",    "data/wav/ok00.wav",
    "data/wav/cancel00.wav", "data/wav/select00.wav", "data/wav/gun00.wav",
    "data/wav/cat00.wav",    "data/wav/lazer00.wav",  "data/wav/lazer01.wav",
    "data/wav/enep01.wav",   "data/wav/nep00.wav",    "data/wav/damage00.wav",
    "data/wav/item00.wav",   "data/wav/kira00.wav",   "data/wav/kira01.wav",
    "data/wav/kira02.wav",   "data/wav/extend.wav",   "data/wav/timeout.wav",
    "data/wav/graze.wav",    "data/wav/powerup.wav",
};

static const M_SOUND m_Sounds[SOUND_COUNT] = {
    { 0, -1500 },  { 0, -2000 },  { 1, -1200 },  { 1, -1400 },  { 2, -1000 },
    { 3, -500 },   { 4, -500 },   { 5, -1700 },  { 6, -1700 },  { 7, -1700 },
    { 8, -1000 },  { 9, -1000 },  { 10, -1900 }, { 11, -1200 }, { 12, -900 },
    { 5, -1500 },  { 13, -900 },  { 14, -900 },  { 15, -600 },  { 16, -400 },
    { 17, -1100 }, { 18, -900 },  { 5, -1800 },  { 6, -1800 },  { 7, -1800 },
    { 19, -300 },  { 20, -600 },  { 21, -800 },  { 22, -100 },  { 23, -500 },
    { 24, -1000 }, { 25, -1000 },
};

static M_PLAYER m_Player;

static float M_Gain(const int32_t hundredths_of_db)
{
    return powf(10.0f, (float)hundredths_of_db / 2000.0f);
}

// Finds chunk id among the chunks at start. The search stops when the
// sizes of the chunks passed add up to limit exactly. Otherwise limit wraps
// below zero, and the search runs on to the end of the file.
static bool M_FindChunk(
    READER *const reader, const size_t start, uint32_t limit,
    const char *const id, uint32_t *const size)
{
    Reader_Seek(reader, start);
    while (limit != 0) {
        char name[4];
        Reader_ReadBytes(reader, name, sizeof(name));
        const uint32_t chunk_size = Reader_ReadU32(reader);
        if (Reader_Failed(reader)) {
            return false;
        }
        if (memcmp(name, id, sizeof(name)) == 0) {
            *size = chunk_size;
            return true;
        }
        limit -= chunk_size + 8;
        Reader_Skip(reader, chunk_size);
    }
    return false;
}

// Checks that reader holds a wave file with a format and a data chunk,
// and leaves it at position 0.
static RESULT M_ReadWave(
    READER *const reader, const char *const path, M_WAVE *const out)
{
    char tag[4];
    Reader_ReadBytes(reader, tag, sizeof(tag));
    FAIL_IF(memcmp(tag, "RIFF", 4) != 0, "%s is not a wave file", path);
    const uint32_t riff_size = Reader_ReadU32(reader);
    Reader_ReadBytes(reader, tag, sizeof(tag));
    FAIL_IF(memcmp(tag, "WAVE", 4) != 0, "%s is not a wave file", path);
    const size_t start = Reader_Tell(reader);
    // The limit is 8 bytes short of the chunks after the WAVE tag, so an
    // empty chunk at the end of the file is not found.
    const uint32_t limit = riff_size - 12;
    uint32_t size;
    FAIL_IF(
        !M_FindChunk(reader, start, limit, "fmt ", &size),
        "%s has no format chunk", path);
    Reader_Skip(reader, 12);
    out->block_align = Reader_ReadU16(reader);
    FAIL_IF(
        !M_FindChunk(reader, start, limit, "data", &size),
        "%s has no data chunk", path);
    out->data_size = (uint32_t)MIN((size_t)size, Reader_Remaining(reader));
    MUST(Reader_Result(reader), "%s", path);
    Reader_Seek(reader, 0);
    return OK;
}

static RESULT M_LoadEffect(const int32_t file)
{
    READER reader;
    MUST(Files_Load(m_Files[file], &reader));
    M_WAVE wave;
    RESULT result = M_ReadWave(&reader, m_Files[file], &wave);
    if (IS_OK(result)) {
        result = AudioSample_Load(&reader, &m_Player.samples[file]);
    }
    Reader_Close(&reader);
    return result;
}

static void M_FreeEffects(void)
{
    for (int32_t i = 0; i < SOUND_COUNT; i++) {
        AudioPlayback_Free(m_Player.playbacks[i]);
        m_Player.playbacks[i] = nullptr;
    }
    for (int32_t i = 0; i < M_FILE_COUNT; i++) {
        AudioSample_Free(m_Player.samples[i]);
        m_Player.samples[i] = nullptr;
    }
    m_Player.effects_loaded = false;
}

static RESULT M_SetExtension(
    const char *const path, const char *const extension, char *const out)
{
    const char *const dot = strrchr(path, '.');
    FAIL_IF(dot == nullptr, "music path %s has no extension", path);
    const size_t stem = (size_t)(dot + 1 - path);
    FAIL_IF(
        stem + strlen(extension) >= SOUND_PATH_SIZE,
        "music path %s is too long", path);
    memcpy(out, path, stem);
    strcpy(out + stem, extension);
    return OK;
}

// Reads the loop of the music from the .pos file at path. A missing or
// short file, and a position outside the audio, leave the loop over the
// whole file at that end.
static AUDIO_LOOP M_ReadLoop(const char *const path, const M_WAVE *const wave)
{
    const uint64_t frames = wave->data_size / wave->block_align;
    AUDIO_LOOP loop = { .enabled = true, .begin = 0, .end = frames };
    READER reader;
    if (!Result_Ignore(Files_Load(path, &reader))) {
        return loop;
    }
    const int64_t begin = (int64_t)Reader_ReadI32(&reader) * M_POS_BYTES;
    const int64_t end = (int64_t)Reader_ReadI32(&reader) * M_POS_BYTES;
    const bool read = !Reader_Failed(&reader);
    Reader_Close(&reader);
    if (!read) {
        return loop;
    }
    if (end > 0 && end < (int64_t)wave->data_size) {
        loop.end = (uint64_t)end / wave->block_align;
    }
    if (begin > 0 && (uint64_t)begin / wave->block_align < loop.end) {
        loop.begin = (uint64_t)begin / wave->block_align;
    }
    return loop;
}

static RESULT M_OpenMusic(const char *const path)
{
    char wave_path[SOUND_PATH_SIZE];
    char pos_path[SOUND_PATH_SIZE];
    MUST(M_SetExtension(path, "wav", wave_path));
    MUST(M_SetExtension(path, "pos", pos_path));
    READER reader;
    MUST(Files_Stream(wave_path, &reader));
    M_WAVE wave;
    RESULT result = M_ReadWave(&reader, wave_path, &wave);
    if (IS_OK(result) && (wave.data_size == 0 || wave.block_align == 0)) {
        result = FAIL("%s has no audio", wave_path);
    }
    if (IS_OK(result)) {
        const AUDIO_PLAY_OPTIONS options = {
            .group = AUDIO_GROUP_MUSIC,
            .volume = 1.0f,
            .pan = 0.0f,
            .pitch = 1.0f,
            .loop = M_ReadLoop(pos_path, &wave),
        };
        result = AudioStream_Load(&reader, &options, &m_Player.music);
    }
    Reader_Close(&reader);
    return result;
}

static void M_StopMusic(void)
{
    if (m_Player.music != nullptr) {
        AudioStream_Free(m_Player.music);
        m_Player.music = nullptr;
    }
    m_Player.music_starting = false;
    m_Player.fading = false;
}

static void M_PlayMusic(const char *const path)
{
    M_StopMusic();
    m_Player.music_starting = Result_Log(M_OpenMusic(path), LOG_LEVEL_WARNING);
}

static void M_StartMusic(void)
{
    if (!m_Player.music_starting) {
        return;
    }
    switch (AudioStream_GetState(m_Player.music)) {
    case AUDIO_LOADING:
        break;
    case AUDIO_READY:
        m_Player.music_starting = false;
        Result_Log(AudioStream_Play(m_Player.music), LOG_LEVEL_WARNING);
        break;
    case AUDIO_FAILED:
        Result_Log(AudioStream_GetResult(m_Player.music), LOG_LEVEL_WARNING);
        M_StopMusic();
        break;
    }
}

static void M_FadeOut(const float seconds)
{
    if (m_Player.music == nullptr) {
        return;
    }
    m_Player.fading = true;
    m_Player.fade_frames = X87_Ftol(seconds * M_FADE_FRAMES_PER_SECOND);
    m_Player.fade_total = m_Player.fade_frames;
}

static void M_StepFade(void)
{
    if (m_Player.music == nullptr || !m_Player.fading) {
        return;
    }
    m_Player.fade_frames--;
    if (m_Player.fade_frames <= 0) {
        m_Player.fading = false;
        m_Player.music_starting = false;
        AudioStream_Stop(m_Player.music);
        return;
    }
    const int32_t volume =
        m_Player.fade_frames * M_FADE_DEPTH / m_Player.fade_total
        - M_FADE_DEPTH;
    Result_Log(
        AudioStream_SetVolume(m_Player.music, M_Gain(volume)),
        LOG_LEVEL_WARNING);
}

static void M_PlayQueued(void)
{
    for (int32_t i = 0; i < SOUND_QUEUE_SIZE; i++) {
        const int32_t sound = g_SoundEvents.queue[i];
        if (sound < 0) {
            break;
        }
        if (!m_Player.effects_loaded || sound >= SOUND_COUNT) {
            continue;
        }
        AUDIO_PLAYBACK **const playback = &m_Player.playbacks[sound];
        if (*playback != nullptr) {
            AudioPlayback_Restart(*playback);
            continue;
        }
        const AUDIO_PLAY_OPTIONS options = {
            .group = AUDIO_GROUP_EFFECTS,
            .volume = M_Gain(m_Sounds[sound].volume),
            .pan = 0.0f,
            .pitch = 1.0f,
        };
        Result_Ignore(AudioSample_Play(
            m_Player.samples[m_Sounds[sound].file], &options, playback));
    }
}

RESULT SoundPlayer_Init(void)
{
    m_Player = (M_PLAYER) {};
    FAIL_IF(!Audio_IsInitialized(), "audio is not initialized");
    m_Player.enabled = true;
    for (int32_t i = 0; i < M_FILE_COUNT; i++) {
        const RESULT result = M_LoadEffect(i);
        if (!IS_OK(result)) {
            M_FreeEffects();
            return result;
        }
    }
    m_Player.effects_loaded = true;
    return OK;
}

void SoundPlayer_Update(void)
{
    if (m_Player.enabled) {
        M_StepFade();
        if (g_SoundEvents.music == SOUND_MUSIC_PLAY) {
            M_PlayMusic(g_SoundEvents.music_path);
        } else if (g_SoundEvents.music == SOUND_MUSIC_STOP) {
            M_StopMusic();
        }
        if (g_SoundEvents.fade_out) {
            M_FadeOut(g_SoundEvents.fade_seconds);
        }
        M_StartMusic();
        M_PlayQueued();
    }
    Sound_Reset();
}

void SoundPlayer_Shutdown(void)
{
    M_StopMusic();
    M_FreeEffects();
    m_Player = (M_PLAYER) {};
}
