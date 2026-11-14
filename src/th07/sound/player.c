#include "th07/sound/player.h"

#include "th07/files.h"
#include "th07/settings.h"
#include "th07/sound/bgm.h"
#include "th07/sound/events.h"

#include "core/audio.h"
#include "core/base/memory.h"
#include "core/math.h"

#include <math.h>
#include <string.h>

#define M_FILE_COUNT 30
#define M_FADE_DEPTH 5000
#define M_FADE_FRAMES_PER_SECOND 60.0f

// Holds an entry of the sound table: its file in m_Files and its volume
// in hundredths of a decibel.
typedef struct {
    int32_t file;
    int16_t volume;
} M_SOUND;

typedef struct {
    bool enabled;
    bool effects_loaded;
    AUDIO_SAMPLE *samples[M_FILE_COUNT];
    AUDIO_PLAYBACK *playbacks[SOUND_COUNT];
    BGM_LIST tracks;
    AUDIO_STREAM *music;
    bool music_starting;
    bool fading;
    int32_t fade_frames;
    int32_t fade_total;
} M_PLAYER;

static const char *const m_Files[M_FILE_COUNT] = {
    "data/wav/se_plst00.wav",   "data/wav/se_enep00.wav",
    "data/wav/se_pldead00.wav", "data/wav/se_power0.wav",
    "data/wav/se_power1.wav",   "data/wav/se_tan00.wav",
    "data/wav/se_tan01.wav",    "data/wav/se_tan02.wav",
    "data/wav/se_ok00.wav",     "data/wav/se_cancel00.wav",
    "data/wav/se_select00.wav", "data/wav/se_gun00.wav",
    "data/wav/se_cat00.wav",    "data/wav/se_lazer00.wav",
    "data/wav/se_lazer01.wav",  "data/wav/se_enep01.wav",
    "data/wav/se_nep00.wav",    "data/wav/se_damage00.wav",
    "data/wav/se_item00.wav",   "data/wav/se_kira00.wav",
    "data/wav/se_kira01.wav",   "data/wav/se_kira02.wav",
    "data/wav/se_extend.wav",   "data/wav/se_timeout.wav",
    "data/wav/se_graze.wav",    "data/wav/se_powerup.wav",
    "data/wav/se_border.wav",   "data/wav/se_bonus.wav",
    "data/wav/se_bonus2.wav",   "data/wav/se_pause.wav",
};

static const M_SOUND m_Sounds[SOUND_COUNT] = {
    { 0, -2000 },  { 0, -2500 },  { 1, -1200 },  { 1, -1500 },  { 2, -1000 },
    { 3, -400 },   { 4, -400 },   { 5, -1500 },  { 6, -1700 },  { 7, -1900 },
    { 8, -1000 },  { 9, -1000 },  { 10, -1700 }, { 11, -1200 }, { 12, -900 },
    { 5, -1500 },  { 13, -900 },  { 14, -900 },  { 15, -900 },  { 16, -200 },
    { 17, -1400 }, { 18, -1300 }, { 5, -100 },   { 6, -1800 },  { 7, -1800 },
    { 19, -800 },  { 20, -1000 }, { 21, -1300 }, { 22, -300 },  { 23, -900 },
    { 24, -900 },  { 25, -500 },  { 26, -300 },  { 27, -300 },  { 24, -300 },
    { 19, 0 },     { 28, -300 },  { 29, -300 },
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
    const char *const id)
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
            return true;
        }
        limit -= chunk_size + 8;
        Reader_Skip(reader, chunk_size);
    }
    return false;
}

// Checks that reader holds a wave file with a format and a data chunk,
// and leaves it at position 0.
static RESULT M_CheckWave(READER *const reader, const char *const path)
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
    FAIL_IF(
        !M_FindChunk(reader, start, limit, "fmt "), "%s has no format chunk",
        path);
    FAIL_IF(
        !M_FindChunk(reader, start, limit, "data"), "%s has no data chunk",
        path);
    MUST(Reader_Result(reader), "%s", path);
    Reader_Seek(reader, 0);
    return OK;
}

static RESULT M_LoadEffect(const int32_t file)
{
    READER reader;
    MUST(Files_Load(m_Files[file], &reader));
    RESULT result = M_CheckWave(&reader, m_Files[file]);
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

static RESULT M_LoadEffects(void)
{
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

static RESULT M_LoadTracks(void)
{
    READER reader;
    MUST(Files_Load(BGM_LIST_PATH, &reader));
    const RESULT result =
        Bgm_ReadList(&reader, &g_HeapAllocator, &m_Player.tracks);
    Reader_Close(&reader);
    return result;
}

static void M_FreeMusic(void)
{
    if (m_Player.music != nullptr) {
        AudioStream_Free(m_Player.music);
        m_Player.music = nullptr;
    }
    m_Player.music_starting = false;
}

static void M_StopMusic(void)
{
    if (m_Player.music != nullptr) {
        AudioStream_Stop(m_Player.music);
    }
    m_Player.music_starting = false;
    m_Player.fading = false;
}

static RESULT M_OpenMusic(const char *const path)
{
    const int32_t track = Bgm_FindTrack(&m_Player.tracks, path);
    READER reader;
    MUST(Files_Stream(BGM_PATH, &reader));
    const RESULT result =
        Bgm_LoadTrack(&m_Player.tracks.tracks[track], &reader, &m_Player.music);
    Reader_Close(&reader);
    return result;
}

static void M_PlayMusic(const char *const path)
{
    M_FreeMusic();
    m_Player.fading = false;
    m_Player.music_starting = Result_Log(M_OpenMusic(path), LOG_LEVEL_WARNING);
}

static void M_StartMusic(void)
{
    if (!m_Player.music_starting || m_Player.music == nullptr) {
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
        M_FreeMusic();
        break;
    }
}

static void M_SetVolume(const int32_t hundredths_of_db)
{
    if (m_Player.music != nullptr) {
        Result_Log(
            AudioStream_SetVolume(m_Player.music, M_Gain(hundredths_of_db)),
            LOG_LEVEL_WARNING);
    }
}

static void M_StepFade(void)
{
    if (!m_Player.fading) {
        return;
    }
    m_Player.fade_frames--;
    if (m_Player.fade_frames <= 0) {
        m_Player.fading = false;
        m_Player.music_starting = false;
        if (m_Player.music != nullptr) {
            AudioStream_Stop(m_Player.music);
        }
        return;
    }
    M_SetVolume(
        m_Player.fade_frames * M_FADE_DEPTH / m_Player.fade_total
        - M_FADE_DEPTH);
}

static void M_FadeOut(const int32_t seconds)
{
    if (m_Player.music == nullptr) {
        return;
    }
    m_Player.fading = true;
    m_Player.fade_frames = X87_Ftol((float)seconds * M_FADE_FRAMES_PER_SECOND);
    m_Player.fade_total = m_Player.fade_frames;
}

static void M_Pause(void)
{
    if (m_Player.music_starting) {
        m_Player.music_starting = false;
    } else if (m_Player.music != nullptr) {
        AudioStream_SetPaused(m_Player.music, true);
    }
}

static void M_Resume(void)
{
    m_Player.music_starting = m_Player.music != nullptr;
}

static void M_ApplyMusic(void)
{
    switch (g_SoundEvents.music) {
    case SOUND_MUSIC_KEEP:
        break;
    case SOUND_MUSIC_PLAY:
        if (m_Player.tracks.count != 0) {
            M_PlayMusic(g_SoundEvents.music_path);
        }
        break;
    case SOUND_MUSIC_STOP:
        M_StopMusic();
        break;
    }
    if (g_SoundEvents.fade_out) {
        M_FadeOut(g_SoundEvents.fade_seconds);
    }
    switch (g_SoundEvents.pause) {
    case SOUND_PAUSE_KEEP:
        break;
    case SOUND_PAUSE_MUSIC:
        M_Pause();
        break;
    case SOUND_RESUME_MUSIC:
        M_Resume();
        break;
    }
}

static void M_PlayQueued(void)
{
    for (int32_t i = 0; i < SOUND_QUEUE_SIZE; i++) {
        const int32_t sound = g_SoundEvents.queue[i];
        if (sound < 0) {
            break;
        }
        g_SoundEvents.queue[i] = -1;
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
    const RESULT effects = M_LoadEffects();
    const RESULT tracks = M_LoadTracks();
    if (!IS_OK(effects)) {
        Result_Ignore(tracks);
        return effects;
    }
    return tracks;
}

void SoundPlayer_Update(void)
{
    if (!m_Player.enabled) {
        Sound_Reset();
        return;
    }
    M_StepFade();
    if (g_Settings.music != SETTINGS_MUSIC_OFF) {
        M_ApplyMusic();
    }
    g_SoundEvents.music = SOUND_MUSIC_KEEP;
    g_SoundEvents.fade_out = false;
    g_SoundEvents.pause = SOUND_PAUSE_KEEP;
    M_StartMusic();
    if (g_Settings.sound) {
        M_PlayQueued();
    }
}

void SoundPlayer_Shutdown(void)
{
    M_FreeMusic();
    M_FreeEffects();
    Bgm_FreeList(&m_Player.tracks);
    m_Player = (M_PLAYER) {};
}
