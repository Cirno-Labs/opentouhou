#include "th07/files.h"
#include "th07/game.h"
#include "th07/settings.h"
#include "th07/sound.h"

#include "core/audio.h"
#include "core/audio/backend.h"
#include "core/io/writer.h"

#include "test.h"
#include "test_dir.h"
#include "th07_data.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#define WAIT_MILLISECONDS 5000
#define EFFECT_FILE_COUNT 30
#define RATE 48000

static const char *const m_EffectFiles[EFFECT_FILE_COUNT] = {
    "plst00", "enep00",  "pldead00", "power0",   "power1",   "tan00",
    "tan01",  "tan02",   "ok00",     "cancel00", "select00", "gun00",
    "cat00",  "lazer00", "lazer01",  "enep01",   "nep00",    "damage00",
    "item00", "kira00",  "kira01",   "kira02",   "extend",   "timeout",
    "graze",  "powerup", "border",   "bonus",    "bonus2",   "pause",
};

// Holds the file and the volume, in hundredths of a decibel, of each sound.
static const int32_t m_SoundTable[SOUND_COUNT][2] = {
    { 0, -2000 },  { 0, -2500 },  { 1, -1200 },  { 1, -1500 },  { 2, -1000 },
    { 3, -400 },   { 4, -400 },   { 5, -1500 },  { 6, -1700 },  { 7, -1900 },
    { 8, -1000 },  { 9, -1000 },  { 10, -1700 }, { 11, -1200 }, { 12, -900 },
    { 5, -1500 },  { 13, -900 },  { 14, -900 },  { 15, -900 },  { 16, -200 },
    { 17, -1400 }, { 18, -1300 }, { 5, -100 },   { 6, -1800 },  { 7, -1800 },
    { 19, -800 },  { 20, -1000 }, { 21, -1300 }, { 22, -300 },  { 23, -900 },
    { 24, -900 },  { 25, -500 },  { 26, -300 },  { 27, -300 },  { 24, -300 },
    { 19, 0 },     { 28, -300 },  { 29, -300 },
};

typedef struct {
    const char *name;
    uint32_t offset;
    uint32_t size;
    int32_t loop_start;
    int32_t loop_end;
    uint16_t channels;
    uint16_t bits;
} TRACK;

static void M_CheckNear(const float actual, const float expected)
{
    if (!isfinite(actual) || fabsf(actual - expected) > 0.00002f) {
        TEST_FAIL("expected %.8f, got %.8f", expected, actual);
    }
}

static float M_Mix(void)
{
    float output[2];
    const Uint64 deadline = SDL_GetTicks() + WAIT_MILLISECONDS;
    while (!AudioBackend_Mix(output, 1)) {
        if (SDL_GetTicks() >= deadline) {
            TEST_FAIL("audio mixer did not acquire its state lock");
            return 0.0f;
        }
        SDL_Delay(1);
    }
    return output[0];
}

static float M_Gain(const int32_t hundredths_of_db)
{
    return powf(10.0f, (float)hundredths_of_db / 2000.0f);
}

// Returns the left sample of frame i of the music: (i + 1) * 64.
static float M_MusicValue(const uint32_t frame)
{
    return (float)((frame + 1) * 64) / 32768.0f;
}

static float M_EffectFileValue(const int32_t file)
{
    return (float)((file + 1) * 1024) / 32768.0f;
}

// Returns frames sample frames of 16-bit stereo PCM: frame i has
// (i + 1) * 64 on the left and silence on the right.
static uint8_t *M_Pcm(const uint32_t frames, size_t *const size)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    for (uint32_t i = 0; i < frames; i++) {
        Writer_WriteI16(&writer, (int16_t)((i + 1) * 64));
        Writer_WriteI16(&writer, 0);
    }
    uint8_t *const data = Writer_TakeBuffer(&writer, size);
    CHECK(Result_Log(Writer_Close(&writer), LOG_LEVEL_ERROR));
    return data;
}

static void M_WriteTrack(WRITER *const writer, const TRACK *const track)
{
    char name[16] = { 0 };
    if (track->name != nullptr) {
        const size_t length = strlen(track->name);
        memcpy(
            name, track->name, length < sizeof(name) ? length : sizeof(name));
    }
    Writer_WriteBytes(writer, name, sizeof(name));
    Writer_WriteU32(writer, track->offset);
    Writer_WriteU32(writer, track->size);
    Writer_WriteI32(writer, track->loop_start);
    Writer_WriteI32(writer, track->loop_end);
    const uint16_t channels = track->channels != 0 ? track->channels : 2;
    const uint16_t bits = track->bits != 0 ? track->bits : 16;
    Writer_WriteU16(writer, 1);
    Writer_WriteU16(writer, channels);
    Writer_WriteU32(writer, RATE);
    Writer_WriteU32(writer, RATE * channels * bits / 8);
    Writer_WriteU16(writer, (uint16_t)(channels * bits / 8));
    Writer_WriteU16(writer, bits);
    Writer_WriteU16(writer, 0);
    Writer_WriteU16(writer, 0);
}

// Returns thbgm.fmt with tracks and then an entry with an empty name,
// unless terminate is false.
static uint8_t *M_List(
    const TRACK *const tracks, const int32_t count, const bool terminate,
    size_t *const size)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    for (int32_t i = 0; i < count; i++) {
        M_WriteTrack(&writer, &tracks[i]);
    }
    if (terminate) {
        M_WriteTrack(&writer, &(TRACK) {});
    }
    uint8_t *const data = Writer_TakeBuffer(&writer, size);
    CHECK(Result_Log(Writer_Close(&writer), LOG_LEVEL_ERROR));
    return data;
}

static RESULT M_ReadList(
    const TRACK *const tracks, const int32_t count, const bool terminate,
    BGM_LIST *const out)
{
    size_t size;
    uint8_t *const data = M_List(tracks, count, terminate, &size);
    READER reader;
    Reader_OpenBuffer(&reader, data, size, nullptr);
    const RESULT result = Bgm_ReadList(&reader, &g_HeapAllocator, out);
    Mem_Free(data);
    return result;
}

static void M_WriteWave(
    const char *const directory, const char *const name, const int16_t value)
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator, nullptr);
    Writer_WriteBytes(&writer, "RIFF", 4);
    Writer_WriteU32(&writer, 36 + 4);
    Writer_WriteBytes(&writer, "WAVEfmt ", 8);
    Writer_WriteU32(&writer, 16);
    Writer_WriteU16(&writer, 1);
    Writer_WriteU16(&writer, 2);
    Writer_WriteU32(&writer, RATE);
    Writer_WriteU32(&writer, RATE * 4);
    Writer_WriteU16(&writer, 4);
    Writer_WriteU16(&writer, 16);
    Writer_WriteBytes(&writer, "data", 4);
    Writer_WriteU32(&writer, 4);
    Writer_WriteI16(&writer, value);
    Writer_WriteI16(&writer, 0);
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    CHECK(Result_Log(Writer_Close(&writer), LOG_LEVEL_ERROR));
    Th07Data_WriteFile(directory, name, data, size);
    Mem_Free(data);
}

// Prepares a data directory: every sound effect file but skip, a skip of
// -1 writing them all, and thbgm.dat with frames frames of music that
// tracks list.
static char *M_Prepare(
    const char *const name, const int32_t skip, const uint32_t frames,
    const TRACK *const tracks, const int32_t count)
{
    char *const directory = TestDir_Prepare(name);
    static const TH07_DATA_FILE version = { "th07_0100b.ver", "0100b" };
    Th07Data_WriteArchive(directory, &version, 1);
    size_t size;
    uint8_t *const pcm = M_Pcm(frames, &size);
    Th07Data_WriteMusic(directory, pcm, size);
    Mem_Free(pcm);
    for (const char *const *dir =
             (const char *const[]) { "data", "data/wav", "bgm", nullptr };
         *dir != nullptr; dir++) {
        char *const path = Str_Format("%s/%s", directory, *dir);
        CHECK(Result_Log(File_MakeDir(path), LOG_LEVEL_ERROR));
        Mem_Free(path);
    }
    uint8_t *const list = M_List(tracks, count, true, &size);
    Th07Data_WriteFile(directory, "bgm/thbgm.fmt", list, size);
    Mem_Free(list);
    for (int32_t i = 0; i < EFFECT_FILE_COUNT; i++) {
        if (i == skip) {
            continue;
        }
        char *const path = Str_Format("data/wav/se_%s.wav", m_EffectFiles[i]);
        M_WriteWave(directory, path, (int16_t)((i + 1) * 1024));
        Mem_Free(path);
    }
    return directory;
}

static RESULT M_Start(const char *const directory)
{
    CHECK(Result_Log(Files_Init(directory), LOG_LEVEL_ERROR));
    CHECK(Result_Log(Audio_Init(), LOG_LEVEL_ERROR));
    Sound_Reset();
    g_Settings.music = SETTINGS_MUSIC_WAVE;
    g_Settings.sound = true;
    g_GameSpeed = 1.0f;
    return SoundPlayer_Init();
}

static void M_Stop(char *const directory)
{
    SoundPlayer_Shutdown();
    Audio_Shutdown();
    Files_Shutdown();
    Sound_Reset();
    Mem_Free(directory);
}

// Queues sound until the player can play it, and returns the first frame
// that it mixes.
static float M_PlayEffect(const int32_t sound)
{
    const Uint64 deadline = SDL_GetTicks() + WAIT_MILLISECONDS;
    while (SDL_GetTicks() < deadline) {
        Sound_Queue(sound);
        SoundPlayer_Update();
        const float output = M_Mix();
        if (output != 0.0f) {
            return output;
        }
        SDL_Delay(1);
    }
    return 0.0f;
}

// Updates the player until the music mixes a frame, and returns it.
static float M_StartMusic(void)
{
    const Uint64 deadline = SDL_GetTicks() + WAIT_MILLISECONDS;
    while (SDL_GetTicks() < deadline) {
        SoundPlayer_Update();
        const float output = M_Mix();
        if (output != 0.0f) {
            return output;
        }
        SDL_Delay(1);
    }
    TEST_FAIL("the music did not start");
    return 0.0f;
}

// Gives the worker time to load music, for checks that it stays silent.
static void M_WaitReady(void)
{
    SDL_Delay(50);
}

static void M_CheckQueue(const int32_t *const expected)
{
    for (int32_t i = 0; i < SOUND_QUEUE_SIZE; i++) {
        CHECK_EQ_INT(g_SoundEvents.queue[i], expected[i]);
    }
}

static const TRACK m_Tracks[] = {
    { "th07_00.wav", 16, 64, 0, 4 * 16, 0, 0 },
    { "th07_01.wav", 16 + 4 * 16, 4 * 32, 4 * 2, 4 * 5, 0, 0 },
    { "th07_02.wav", 16 + 4 * 32, 4 * 16, 0, 4 * 3, 0, 0 },
};

TEST(queue_holds_five_sounds_in_order)
{
    Sound_Reset();
    const int32_t sounds[] = { SOUND_GRAZE,  SOUND_ITEM,  SOUND_DAMAGE,
                               SOUND_BORDER, SOUND_PAUSE, SOUND_EXTEND };
    for (size_t i = 0; i < ARRAY_SIZE(sounds); i++) {
        Sound_Queue(sounds[i]);
    }
    M_CheckQueue(sounds);
}

TEST(queue_skips_a_sound_already_queued_and_takes_indices_up_to_0x7f)
{
    Sound_Reset();
    Sound_Queue(SOUND_GRAZE);
    Sound_Queue(SOUND_GRAZE);
    Sound_Queue(-1);
    Sound_Queue(0x80);
    Sound_Queue(0x7f);
    Sound_Queue(SOUND_COUNT);
    M_CheckQueue((const int32_t[]) { SOUND_GRAZE, 0x7f, SOUND_COUNT, -1, -1 });
}

TEST(play_names_the_wav_track_and_ignores_a_path_without_a_dot)
{
    Sound_Reset();
    Sound_PlayMusic("bgm/th07_13b.mid");
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_PLAY);
    CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/th07_13b.wav");
    Sound_PlayMusic("bgm/x.m");
    CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/x.wav");
    Sound_PlayMusic("bgm/x.midi");
    CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/x.wavi");
    Sound_PlayMusic("bgm/nodot");
    CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/x.wavi");
    char path[SOUND_PATH_SIZE];
    memset(path, 'a', sizeof(path) - 5);
    memcpy(path + sizeof(path) - 5, ".mid", 5);
    Sound_PlayMusic(path);
    CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/x.wavi");
}

TEST(play_and_stop_drop_an_earlier_fade_and_pause)
{
    Sound_Reset();
    Sound_FadeOutMusic(3.0f);
    Sound_PauseMusic();
    Sound_PlayMusic("bgm/th07_01.mid");
    CHECK(!g_SoundEvents.fade_out);
    CHECK_EQ_INT(g_SoundEvents.pause, SOUND_PAUSE_KEEP);
    Sound_FadeOutMusic(3.0f);
    Sound_ResumeMusic();
    Sound_StopMusic();
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_STOP);
    CHECK(!g_SoundEvents.fade_out);
    CHECK_EQ_INT(g_SoundEvents.pause, SOUND_PAUSE_KEEP);
    Sound_FadeOutMusic(3.0f);
    CHECK(!g_SoundEvents.fade_out);
}

TEST(the_later_of_pause_and_resume_wins)
{
    Sound_Reset();
    Sound_PauseMusic();
    CHECK_EQ_INT(g_SoundEvents.pause, SOUND_PAUSE_MUSIC);
    Sound_ResumeMusic();
    CHECK_EQ_INT(g_SoundEvents.pause, SOUND_RESUME_MUSIC);
    Sound_PauseMusic();
    CHECK_EQ_INT(g_SoundEvents.pause, SOUND_PAUSE_MUSIC);
}

TEST(fade_seconds_stretch_while_slow_and_cut_to_whole_seconds)
{
    const float speeds[] = { 1.0f, 0.5f, 0.3f, 2.0f, 0.0f };
    const int32_t expected[] = { 3, 7, 11, 3, 3 };
    for (size_t i = 0; i < ARRAY_SIZE(speeds); i++) {
        Sound_Reset();
        g_GameSpeed = speeds[i];
        Sound_FadeOutMusic(3.5f);
        CHECK(g_SoundEvents.fade_out);
        CHECK_EQ_INT(g_SoundEvents.fade_seconds, expected[i]);
    }
    g_GameSpeed = 1.0f;
}

TEST(list_reads_the_tracks_up_to_the_empty_name)
{
    BGM_LIST list;
    CHECK(Result_Log(M_ReadList(m_Tracks, 3, true, &list), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(list.count, 3);
    const BGM_TRACK *const track = &list.tracks[1];
    CHECK_EQ_STR(track->name, "th07_01.wav");
    CHECK_EQ_INT(track->offset, 80);
    CHECK_EQ_INT(track->size, 128);
    CHECK_EQ_INT(track->loop_start, 8);
    CHECK_EQ_INT(track->loop_end, 20);
    CHECK_EQ_INT(track->format_tag, 1);
    CHECK_EQ_INT(track->channels, 2);
    CHECK_EQ_INT(track->sample_rate, RATE);
    CHECK_EQ_INT(track->block_align, 4);
    CHECK_EQ_INT(track->bits_per_sample, 16);
    Bgm_FreeList(&list);
    CHECK(list.tracks == nullptr);
}

TEST(list_ends_at_an_end_entry_of_one_byte)
{
    size_t size;
    uint8_t *const tracks = M_List(m_Tracks, 3, false, &size);
    uint8_t *const data = Mem_Alloc(size + 1);
    memcpy(data, tracks, size);
    data[size] = 0;
    READER reader;
    Reader_OpenBuffer(&reader, data, size + 1, nullptr);
    BGM_LIST list;
    CHECK(Result_Log(
        Bgm_ReadList(&reader, &g_HeapAllocator, &list), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(list.count, 3);
    Bgm_FreeList(&list);
    Mem_Free(data);
    Mem_Free(tracks);
}

TEST(list_without_an_end_entry_or_a_track_fails)
{
    BGM_LIST list;
    CHECK(!Result_Ignore(M_ReadList(m_Tracks, 3, false, &list)));
    CHECK(!Result_Ignore(M_ReadList(m_Tracks, 0, true, &list)));
    const TRACK long_name = { "th07_01.wav_long", 0, 4, 0, 4, 0, 0 };
    CHECK(!Result_Ignore(M_ReadList(&long_name, 1, true, &list)));
    CHECK(list.tracks == nullptr);
}

TEST(find_track_takes_the_file_name_and_falls_back_to_the_first)
{
    BGM_LIST list;
    CHECK(Result_Log(M_ReadList(m_Tracks, 3, true, &list), LOG_LEVEL_ERROR));
    CHECK_EQ_INT(Bgm_FindTrack(&list, "bgm/th07_02.wav"), 2);
    CHECK_EQ_INT(Bgm_FindTrack(&list, "a\\b\\th07_01.wav"), 1);
    CHECK_EQ_INT(Bgm_FindTrack(&list, "th07_01.wav"), 1);
    CHECK_EQ_INT(Bgm_FindTrack(&list, "bgm/th07_09.wav"), 0);
    CHECK_EQ_INT(Bgm_FindTrack(&list, "bgm/th07_02.mid"), 0);
    Bgm_FreeList(&list);
}

TEST(each_sound_plays_its_file_at_its_volume)
{
    char *const directory = M_Prepare("volumes", -1, 16, m_Tracks, 1);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    for (int32_t sound = 0; sound < SOUND_COUNT; sound++) {
        const float expected = M_EffectFileValue(m_SoundTable[sound][0])
            * M_Gain(m_SoundTable[sound][1]);
        M_CheckNear(M_PlayEffect(sound), expected);
    }
    M_Stop(directory);
}

TEST(sounds_stay_queued_while_sound_is_off)
{
    char *const directory = M_Prepare("sound_off", -1, 16, m_Tracks, 1);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    g_Settings.sound = false;
    Sound_Queue(SOUND_GRAZE);
    SoundPlayer_Update();
    M_CheckNear(M_Mix(), 0.0f);
    CHECK_EQ_INT(g_SoundEvents.queue[0], SOUND_GRAZE);
    g_Settings.sound = true;
    M_CheckNear(
        M_PlayEffect(SOUND_GRAZE), M_EffectFileValue(24) * M_Gain(-900));
    M_Stop(directory);
}

TEST(a_missing_effect_file_silences_every_effect_but_not_the_music)
{
    char *const directory = M_Prepare("missing_effect", 29, 16, m_Tracks, 1);
    CHECK(!Result_Ignore(M_Start(directory)));
    for (int32_t i = 0; i < 10; i++) {
        Sound_Queue(SOUND_PLAYER_SHOT);
        SoundPlayer_Update();
        M_CheckNear(M_Mix(), 0.0f);
    }
    Sound_PlayMusic("bgm/th07_00.mid");
    M_CheckNear(M_StartMusic(), M_MusicValue(0));
    M_Stop(directory);
}

TEST(play_opens_the_track_in_the_next_update_and_starts_it_once_ready)
{
    char *const directory = M_Prepare("play_start", -1, 64, m_Tracks, 3);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    Sound_PlayMusic("bgm/th07_02.mid");
    SoundPlayer_Update();
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_KEEP);
    M_CheckNear(M_StartMusic(), M_MusicValue(32));
    M_CheckNear(M_Mix(), M_MusicValue(33));
    M_Stop(directory);
}

TEST(play_replaces_the_music_that_plays)
{
    char *const directory = M_Prepare("play_replace", -1, 64, m_Tracks, 3);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    Sound_PlayMusic("bgm/th07_00.mid");
    M_CheckNear(M_StartMusic(), M_MusicValue(0));
    M_CheckNear(M_Mix(), M_MusicValue(1));
    Sound_PlayMusic("bgm/th07_02.mid");
    M_CheckNear(M_StartMusic(), M_MusicValue(32));
    M_Stop(directory);
}

TEST(a_track_plays_to_its_loop_end_and_repeats_from_its_loop_start)
{
    char *const directory = M_Prepare("loop", -1, 64, m_Tracks, 3);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    Sound_PlayMusic("bgm/th07_01.mid");
    M_CheckNear(M_StartMusic(), M_MusicValue(16));
    const uint32_t expected[] = { 17, 18, 19, 20, 18, 19, 20, 18 };
    for (size_t i = 0; i < ARRAY_SIZE(expected); i++) {
        M_CheckNear(M_Mix(), M_MusicValue(expected[i]));
    }
    M_Stop(directory);
}

TEST(a_track_without_loop_start_repeats_from_its_start)
{
    char *const directory = M_Prepare("loop_zero", -1, 64, m_Tracks, 3);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    Sound_PlayMusic("bgm/th07_02.mid");
    M_CheckNear(M_StartMusic(), M_MusicValue(32));
    const uint32_t expected[] = { 33, 34, 32, 33, 34, 32 };
    for (size_t i = 0; i < ARRAY_SIZE(expected); i++) {
        M_CheckNear(M_Mix(), M_MusicValue(expected[i]));
    }
    M_Stop(directory);
}

TEST(an_unknown_track_plays_the_first)
{
    char *const directory = M_Prepare("unknown", -1, 64, m_Tracks, 3);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    Sound_PlayMusic("bgm/th07_99.mid");
    M_CheckNear(M_StartMusic(), M_MusicValue(0));
    M_Stop(directory);
}

TEST(pause_silences_the_music_resume_plays_it_on_and_stop_ends_it)
{
    char *const directory = M_Prepare("stop", -1, 64, m_Tracks, 3);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    Sound_PlayMusic("bgm/th07_00.mid");
    M_CheckNear(M_StartMusic(), M_MusicValue(0));
    M_CheckNear(M_Mix(), M_MusicValue(1));
    Sound_PauseMusic();
    SoundPlayer_Update();
    M_CheckNear(M_Mix(), 0.0f);
    Sound_ResumeMusic();
    M_CheckNear(M_StartMusic(), M_MusicValue(2));
    Sound_StopMusic();
    SoundPlayer_Update();
    M_CheckNear(M_Mix(), 0.0f);
    SoundPlayer_Update();
    M_CheckNear(M_Mix(), 0.0f);
    M_Stop(directory);
}

TEST(music_off_ignores_the_music_events)
{
    char *const directory = M_Prepare("music_off", -1, 64, m_Tracks, 3);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    Sound_PlayMusic("bgm/th07_00.mid");
    M_CheckNear(M_StartMusic(), M_MusicValue(0));
    g_Settings.music = SETTINGS_MUSIC_OFF;
    Sound_PauseMusic();
    SoundPlayer_Update();
    M_CheckNear(M_Mix(), M_MusicValue(1));
    Sound_FadeOutMusic(1.0f);
    SoundPlayer_Update();
    M_CheckNear(M_Mix(), M_MusicValue(2));
    Sound_StopMusic();
    SoundPlayer_Update();
    M_CheckNear(M_Mix(), M_MusicValue(3));
    Sound_PlayMusic("bgm/th07_02.mid");
    SoundPlayer_Update();
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_KEEP);
    for (uint32_t i = 4; i < 70; i++) {
        SoundPlayer_Update();
        M_CheckNear(M_Mix(), M_MusicValue(i % 16));
    }
    M_Stop(directory);
}

// Fades over 1 second, which is 60 ticks: tick n sets the volume to
// (60 - n) * 5000 / 60 - 5000 hundredths of a decibel, rounded towards
// zero, and tick 60 stops the music.
TEST(fade_lowers_the_volume_each_tick_and_then_stops)
{
    const TRACK track = { "th07_00.wav", 16, 0, 0, 4 * 4096, 0, 0 };
    char *const directory = M_Prepare("fade", -1, 4096, &track, 1);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    Sound_PlayMusic("bgm/th07_00.mid");
    M_CheckNear(M_StartMusic(), M_MusicValue(0));
    Sound_FadeOutMusic(1.9f);
    SoundPlayer_Update();
    M_CheckNear(M_Mix(), M_MusicValue(1));
    for (uint32_t n = 1; n < 60; n++) {
        SoundPlayer_Update();
        const int32_t volume = (int32_t)(60 - n) * 5000 / 60 - 5000;
        M_CheckNear(M_Mix(), M_MusicValue(n + 1) * M_Gain(volume));
    }
    SoundPlayer_Update();
    M_CheckNear(M_Mix(), 0.0f);
    M_Stop(directory);
}

TEST(a_fade_under_a_second_stops_on_the_next_update)
{
    char *const directory = M_Prepare("short_fade", -1, 64, m_Tracks, 1);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    Sound_PlayMusic("bgm/th07_00.mid");
    M_CheckNear(M_StartMusic(), M_MusicValue(0));
    Sound_FadeOutMusic(0.9f);
    SoundPlayer_Update();
    M_CheckNear(M_Mix(), M_MusicValue(1));
    SoundPlayer_Update();
    M_CheckNear(M_Mix(), 0.0f);
    M_Stop(directory);
}

TEST(a_track_that_cannot_be_played_leaves_silence)
{
    const TRACK tracks[] = {
        { "end0.wav", 16, 64, 0, 0, 0, 0 },
        { "odd.wav", 16, 64, 0, 6, 0, 0 },
        { "past.wav", 16, 64, 0, 4 * 64 + 4, 0, 0 },
        { "mono8.wav", 16, 64, 0, 4, 1, 8 },
        { "late.wav", 16, 64, 8, 8, 0, 0 },
    };
    char *const directory = M_Prepare("bad_tracks", -1, 64, tracks, 5);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    const char *const paths[] = { "end0.mid", "odd.mid", "past.mid",
                                  "mono8.mid", "late.mid" };
    for (size_t i = 0; i < ARRAY_SIZE(paths); i++) {
        Sound_PlayMusic(paths[i]);
        for (int32_t tick = 0; tick < 10; tick++) {
            SoundPlayer_Update();
            M_WaitReady();
            M_CheckNear(M_Mix(), 0.0f);
        }
    }
    M_Stop(directory);
}

TEST(player_without_audio_reports_failure_and_clears_the_events)
{
    Sound_Reset();
    g_Settings.music = SETTINGS_MUSIC_WAVE;
    CHECK(!Result_Ignore(SoundPlayer_Init()));
    Sound_Queue(SOUND_GRAZE);
    Sound_PlayMusic("bgm/th07_01.mid");
    SoundPlayer_Update();
    CHECK_EQ_INT(g_SoundEvents.queue[0], -1);
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_KEEP);
    SoundPlayer_Shutdown();
}
