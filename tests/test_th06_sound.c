// SPDX-License-Identifier: GPL-3.0-only

#include "th06/files.h"
#include "th06/settings.h"
#include "th06/sound.h"

#include "core/audio.h"
#include "core/audio/backend.h"
#include "core/io/writer.h"

#include "test.h"
#include "test_dir.h"
#include "th06_data.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#define WAIT_MILLISECONDS 5000
#define EFFECT_FILE_COUNT 26

static const char *const m_EffectFiles[EFFECT_FILE_COUNT] = {
    "plst00",  "enep00", "pldead00", "power0",   "power1",  "tan00",  "tan01",
    "tan02",   "ok00",   "cancel00", "select00", "gun00",   "cat00",  "lazer00",
    "lazer01", "enep01", "nep00",    "damage00", "item00",  "kira00", "kira01",
    "kira02",  "extend", "timeout",  "graze",    "powerup",
};

// Holds the file and the volume, in hundredths of a decibel, of each sound.
static const int32_t m_SoundTable[SOUND_COUNT][2] = {
    { 0, -1500 },  { 0, -2000 },  { 1, -1200 },  { 1, -1400 },  { 2, -1000 },
    { 3, -500 },   { 4, -500 },   { 5, -1700 },  { 6, -1700 },  { 7, -1700 },
    { 8, -1000 },  { 9, -1000 },  { 10, -1900 }, { 11, -1200 }, { 12, -900 },
    { 5, -1500 },  { 13, -900 },  { 14, -900 },  { 15, -600 },  { 16, -400 },
    { 17, -1100 }, { 18, -900 },  { 5, -1800 },  { 6, -1800 },  { 7, -1800 },
    { 19, -300 },  { 20, -600 },  { 21, -800 },  { 22, -100 },  { 23, -500 },
    { 24, -1000 }, { 25, -1000 },
};

static void M_CheckNear(const float actual, const float expected)
{
    if (!isfinite(actual) || fabsf(actual - expected) > 0.00002f) {
        TEST_FAIL("expected %.8f, got %.8f", expected, actual);
    }
}

static void M_CheckQueue(const int32_t a, const int32_t b, const int32_t c)
{
    CHECK_EQ_INT(g_SoundEvents.queue[0], a);
    CHECK_EQ_INT(g_SoundEvents.queue[1], b);
    CHECK_EQ_INT(g_SoundEvents.queue[2], c);
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

// Writes a 16-bit stereo wave file at 48 kHz. Frame i has value(i) as its
// left sample and silence as its right one.
static void M_WriteWave(
    const char *const directory, const char *const name, const uint32_t frames,
    int16_t (*const value)(uint32_t))
{
    WRITER writer;
    Writer_OpenBuffer(&writer, &g_HeapAllocator);
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
        Writer_WriteI16(&writer, value(i));
        Writer_WriteI16(&writer, 0);
    }
    size_t size;
    uint8_t *const data = Writer_TakeBuffer(&writer, &size);
    CHECK(Result_Log(Writer_Close(&writer), LOG_LEVEL_ERROR));
    Th06Data_WriteFile(directory, name, data, size);
    Mem_Free(data);
}

static int32_t m_EffectValue;

static int16_t M_EffectValue(const uint32_t frame)
{
    return (int16_t)m_EffectValue;
}

static int16_t M_Ramp(const uint32_t frame)
{
    return (int16_t)((frame + 1) * 1024);
}

static int16_t M_Half(const uint32_t frame)
{
    return 16384;
}

static float M_RampValue(const uint32_t frame)
{
    return (float)((frame + 1) * 1024) / 32768.0f;
}

static float M_EffectFileValue(const int32_t file)
{
    return (float)((file + 1) * 1024) / 32768.0f;
}

static float M_Gain(const int32_t hundredths_of_db)
{
    return powf(10.0f, (float)hundredths_of_db / 2000.0f);
}

// Prepares a data directory with every sound effect file except skip. A
// skip of -1 writes them all.
static char *M_Prepare(const char *const name, const int32_t skip)
{
    char *const directory = TestDir_Prepare(name);
    Th06Data_WriteArchives(directory, -1);
    char *const wav = Str_Format("%s/data/wav", directory);
    CHECK(Result_Log(File_MakeDir(wav), LOG_LEVEL_ERROR));
    Mem_Free(wav);
    char *const bgm = Str_Format("%s/bgm", directory);
    CHECK(Result_Log(File_MakeDir(bgm), LOG_LEVEL_ERROR));
    Mem_Free(bgm);
    for (int32_t i = 0; i < EFFECT_FILE_COUNT; i++) {
        if (i == skip) {
            continue;
        }
        char *const path = Str_Format("data/wav/%s.wav", m_EffectFiles[i]);
        m_EffectValue = (i + 1) * 1024;
        M_WriteWave(directory, path, 1, M_EffectValue);
        Mem_Free(path);
    }
    return directory;
}

// Starts the files of directory, the audio and the player.
static RESULT M_Start(const char *const directory)
{
    CHECK(Result_Log(Files_Init(directory), LOG_LEVEL_ERROR));
    CHECK(Result_Log(Audio_Init(), LOG_LEVEL_ERROR));
    Sound_Reset();
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

static void M_WritePos(
    const char *const directory, const char *const name, const int32_t begin,
    const int32_t end)
{
    uint8_t data[8];
    for (int32_t i = 0; i < 4; i++) {
        data[i] = (uint8_t)((uint32_t)begin >> (i * 8));
        data[4 + i] = (uint8_t)((uint32_t)end >> (i * 8));
    }
    Th06Data_WriteFile(directory, name, data, sizeof(data));
}

TEST(queue_holds_three_sounds_in_order)
{
    Sound_Reset();
    Sound_Queue(SOUND_GRAZE);
    Sound_Queue(SOUND_ITEM);
    Sound_Queue(SOUND_DAMAGE);
    Sound_Queue(SOUND_EXTEND);
    M_CheckQueue(SOUND_GRAZE, SOUND_ITEM, SOUND_DAMAGE);
}

TEST(queue_skips_a_sound_already_queued)
{
    Sound_Reset();
    Sound_Queue(SOUND_GRAZE);
    Sound_Queue(SOUND_GRAZE);
    Sound_Queue(SOUND_ITEM);
    M_CheckQueue(SOUND_GRAZE, SOUND_ITEM, -1);
}

TEST(queue_gives_a_place_to_indices_up_to_0x7f_only)
{
    Sound_Reset();
    Sound_Queue(-1);
    Sound_Queue(0x80);
    Sound_Queue(0x7f);
    Sound_Queue(SOUND_COUNT);
    M_CheckQueue(0x7f, SOUND_COUNT, -1);
}

TEST(play_music_records_the_path_and_drops_an_earlier_fade)
{
    Sound_Reset();
    Sound_FadeOutMusic(4.0f);
    Sound_PlayMusic("bgm/th06_02.mid");
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_PLAY);
    CHECK_EQ_STR(g_SoundEvents.music_path, "bgm/th06_02.mid");
    CHECK(!g_SoundEvents.fade_out);
    Sound_FadeOutMusic(4.0f);
    CHECK(g_SoundEvents.fade_out);
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_PLAY);
}

TEST(stop_music_drops_a_fade_and_a_later_fade_does_nothing)
{
    Sound_Reset();
    Sound_FadeOutMusic(4.0f);
    Sound_StopMusic();
    CHECK(!g_SoundEvents.fade_out);
    Sound_FadeOutMusic(4.0f);
    CHECK(!g_SoundEvents.fade_out);
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_STOP);
}

TEST(play_music_ignores_a_path_too_long)
{
    Sound_Reset();
    char path[SOUND_PATH_SIZE + 1];
    memset(path, 'a', SOUND_PATH_SIZE);
    path[SOUND_PATH_SIZE] = '\0';
    Sound_PlayMusic(path);
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_KEEP);
}

TEST(player_without_audio_reports_failure_and_clears_the_events)
{
    Sound_Reset();
    CHECK(!Result_Ignore(SoundPlayer_Init()));
    Sound_Queue(SOUND_GRAZE);
    Sound_PlayMusic("bgm/th06_01.mid");
    SoundPlayer_Update();
    M_CheckQueue(-1, -1, -1);
    CHECK_EQ_INT(g_SoundEvents.music, SOUND_MUSIC_KEEP);
    SoundPlayer_Shutdown();
}

TEST(each_sound_plays_its_file_at_its_volume)
{
    char *const directory = M_Prepare("volumes", -1);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    for (int32_t sound = 0; sound < SOUND_COUNT; sound++) {
        const float expected = M_EffectFileValue(m_SoundTable[sound][0])
            * M_Gain(m_SoundTable[sound][1]);
        M_CheckNear(M_PlayEffect(sound), expected);
    }
    M_Stop(directory);
}

TEST(a_sound_queued_again_starts_from_the_beginning)
{
    char *const directory = M_Prepare("restart", -1);
    M_WriteWave(directory, "data/wav/graze.wav", 4, M_Ramp);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    const float gain = M_Gain(-1000);
    M_CheckNear(M_PlayEffect(SOUND_GRAZE), M_RampValue(0) * gain);
    M_CheckNear(M_Mix(), M_RampValue(1) * gain);
    Sound_Queue(SOUND_GRAZE);
    SoundPlayer_Update();
    M_CheckNear(M_Mix(), M_RampValue(0) * gain);
    M_Stop(directory);
}

TEST(a_missing_effect_file_silences_every_effect_but_not_the_music)
{
    char *const directory = M_Prepare("missing_effect", 25);
    M_WriteWave(directory, "bgm/th06_01.wav", 8, M_Ramp);
    CHECK(!Result_Ignore(M_Start(directory)));
    for (int32_t i = 0; i < 20; i++) {
        Sound_Queue(SOUND_PLAYER_SHOT);
        SoundPlayer_Update();
        M_CheckNear(M_Mix(), 0.0f);
        SDL_Delay(1);
    }
    Sound_PlayMusic("bgm/th06_01.mid");
    M_CheckNear(M_StartMusic(), M_RampValue(0));
    M_Stop(directory);
}

TEST(an_empty_last_chunk_is_not_found)
{
    char *const directory = M_Prepare("empty_last_chunk", -1);
    M_WriteWave(directory, "data/wav/graze.wav", 0, M_Ramp);
    CHECK(!Result_Ignore(M_Start(directory)));
    M_Stop(directory);
}

TEST(music_plays_the_wave_file_and_repeats_between_the_pos_positions)
{
    char *const directory = M_Prepare("music_loop", -1);
    M_WriteWave(directory, "bgm/th06_01.wav", 16, M_Ramp);
    M_WritePos(directory, "bgm/th06_01.pos", 1, 3);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    Sound_PlayMusic("bgm/th06_01.mid");
    M_CheckNear(M_StartMusic(), M_RampValue(0));
    const uint32_t expected[] = { 1, 2, 1, 2, 1, 2 };
    for (size_t i = 0; i < ARRAY_SIZE(expected); i++) {
        M_CheckNear(M_Mix(), M_RampValue(expected[i]));
    }
    M_Stop(directory);
}

TEST(music_without_a_pos_file_repeats_the_whole_file)
{
    char *const directory = M_Prepare("music_no_pos", -1);
    M_WriteWave(directory, "bgm/th06_01.wav", 3, M_Ramp);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    Sound_PlayMusic("bgm/th06_01.mid");
    M_CheckNear(M_StartMusic(), M_RampValue(0));
    const uint32_t expected[] = { 1, 2, 0, 1, 2, 0 };
    for (size_t i = 0; i < ARRAY_SIZE(expected); i++) {
        M_CheckNear(M_Mix(), M_RampValue(expected[i]));
    }
    M_Stop(directory);
}

TEST(a_pos_end_past_the_audio_repeats_to_the_end_of_the_file)
{
    char *const directory = M_Prepare("music_long_pos", -1);
    M_WriteWave(directory, "bgm/th06_01.wav", 3, M_Ramp);
    M_WritePos(directory, "bgm/th06_01.pos", 1, 100);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    Sound_PlayMusic("bgm/th06_01.mid");
    M_CheckNear(M_StartMusic(), M_RampValue(0));
    const uint32_t expected[] = { 1, 2, 1, 2 };
    for (size_t i = 0; i < ARRAY_SIZE(expected); i++) {
        M_CheckNear(M_Mix(), M_RampValue(expected[i]));
    }
    M_Stop(directory);
}

TEST(stop_music_silences_it)
{
    char *const directory = M_Prepare("music_stop", -1);
    M_WriteWave(directory, "bgm/th06_01.wav", 4096, M_Half);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    Sound_PlayMusic("bgm/th06_01.mid");
    M_CheckNear(M_StartMusic(), 0.5f);
    Sound_StopMusic();
    SoundPlayer_Update();
    M_CheckNear(M_Mix(), 0.0f);
    M_Stop(directory);
}

// Fades over 0.1 seconds, which are 6 ticks: tick n sets the volume to
// (6 - n) * 5000 / 6 - 5000 hundredths of a decibel, rounded towards zero,
// and tick 6 stops the music.
TEST(fade_lowers_the_volume_each_tick_and_then_stops)
{
    char *const directory = M_Prepare("music_fade", -1);
    M_WriteWave(directory, "bgm/th06_01.wav", 4096, M_Half);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    Sound_PlayMusic("bgm/th06_01.mid");
    M_CheckNear(M_StartMusic(), 0.5f);
    Sound_FadeOutMusic(0.1f);
    SoundPlayer_Update();
    M_CheckNear(M_Mix(), 0.5f);
    const int32_t volumes[] = { -834, -1667, -2500, -3334, -4167 };
    for (size_t i = 0; i < ARRAY_SIZE(volumes); i++) {
        SoundPlayer_Update();
        M_CheckNear(M_Mix(), 0.5f * M_Gain(volumes[i]));
    }
    SoundPlayer_Update();
    M_CheckNear(M_Mix(), 0.0f);
    M_Stop(directory);
}

TEST(play_music_after_a_fade_starts_at_full_volume)
{
    char *const directory = M_Prepare("music_after_fade", -1);
    M_WriteWave(directory, "bgm/th06_01.wav", 4096, M_Half);
    M_WriteWave(directory, "bgm/th06_02.wav", 4096, M_Half);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    Sound_PlayMusic("bgm/th06_01.mid");
    M_CheckNear(M_StartMusic(), 0.5f);
    Sound_FadeOutMusic(4.0f);
    SoundPlayer_Update();
    SoundPlayer_Update();
    Sound_PlayMusic("bgm/th06_02.mid");
    M_CheckNear(M_StartMusic(), 0.5f);
    SoundPlayer_Update();
    M_CheckNear(M_Mix(), 0.5f);
    M_Stop(directory);
}

TEST(sound_off_plays_no_effect_and_starts_no_music)
{
    char *const directory = M_Prepare("sound_off", -1);
    M_WriteWave(directory, "bgm/th06_01.wav", 16, M_Ramp);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    g_Settings.sound = false;
    Sound_PlayMusic("bgm/th06_01.mid");
    bool heard = false;
    for (int32_t i = 0; i < 50; i++) {
        Sound_Queue(SOUND_OK);
        SoundPlayer_Update();
        heard = heard || M_Mix() != 0.0f;
        SDL_Delay(1);
    }
    CHECK(!heard);
    g_Settings.sound = true;
    M_Stop(directory);
}

TEST(music_off_ignores_the_music_but_plays_effects)
{
    char *const directory = M_Prepare("music_off", -1);
    M_WriteWave(directory, "bgm/th06_01.wav", 16, M_Ramp);
    CHECK(Result_Log(M_Start(directory), LOG_LEVEL_ERROR));
    g_Settings.music = SETTINGS_MUSIC_OFF;
    Sound_PlayMusic("bgm/th06_01.mid");
    for (int32_t i = 0; i < 20; i++) {
        SoundPlayer_Update();
        SDL_Delay(1);
    }
    CHECK(M_Mix() == 0.0f);
    CHECK(M_PlayEffect(SOUND_OK) != 0.0f);
    g_Settings.music = SETTINGS_MUSIC_WAVE;
    M_Stop(directory);
}

TEST(settings_default_to_the_original_configuration)
{
    TH06_SETTINGS settings;
    Config_SetDefaults(&g_SettingsSchema, &settings);
    CHECK_EQ_INT(settings.lives, 2);
    CHECK_EQ_INT(settings.bombs, 3);
    CHECK_EQ_INT(settings.difficulty, 1);
    CHECK_EQ_INT(settings.music, SETTINGS_MUSIC_WAVE);
    CHECK(settings.sound);
    CHECK_EQ_INT(g_Settings.lives, settings.lives);
    CHECK_EQ_INT(g_Settings.bombs, settings.bombs);
    CHECK_EQ_INT(g_Settings.difficulty, settings.difficulty);
    CHECK_EQ_INT(g_Settings.music, settings.music);
    CHECK(g_Settings.sound);
}
