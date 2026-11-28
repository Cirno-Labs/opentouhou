#include "th02/program/private.h"

#include <string.h>

static void M_StopMusic(void)
{
    AudioClock_Advance(TH02_CLOCK_STOP);
    MusicWait_Stop(&p_Program->audio.music_wait);
    Pmd_Stop(p_Program->audio.pmd);
}

static void M_LoadMusic(const MUSIC_ID song)
{
    if (!AudioClock_Failed()) {
        AudioClock_Advance(TH02_CLOCK_LOAD);
        MusicWait_Stop(&p_Program->audio.music_wait);
        p_Program->audio.music_loaded = true;
        Pmd_Load(p_Program->audio.pmd, p_Program->assets.music.songs[song]);
    }
}

static void M_StartMusic(void)
{
    if (AudioClock_Failed()) {
        return;
    }
    if (p_Program->audio.sound.fm_possible && p_Program->audio.music_loaded) {
        AudioClock_Advance(TH02_CLOCK_START);
        MusicWait_Start(&p_Program->audio.music_wait);
        Pmd_Start(p_Program->audio.pmd);
    } else {
        M_StopMusic();
    }
}

static void M_TitleMusic(
    void *const context, const TITLE_AUDIO_REQUEST *const request)
{
    if (request->midi) {
        return;
    }
    if (request->action == TITLE_STOP_SONG) {
        if (p_Program->audio.sound.active) {
            M_StopMusic();
        }
    } else if (request->action == TITLE_LOAD_MENU_SONG) {
        M_LoadMusic(MUSIC_OP);
    } else if (request->action == TITLE_PLAY_SONG) {
        M_StartMusic();
    } else if (request->action == TITLE_FORCE_EFFECT) {
        SoundEffects_Force(request->effect);
    }
}

static void M_RoomMusic(
    void *const context, const MUSIC_ROOM_AUDIO_REQUEST *const request)
{
    static const MUSIC_ID songs[MUSIC_ROOM_TRACK_COUNT] = {
        MUSIC_OP,     MUSIC_STAGE0, MUSIC_STAGE1, MUSIC_STAGE2, MUSIC_STAGE3,
        MUSIC_STAGE4, MUSIC_BOSS1,  MUSIC_BOSS4,  MUSIC_BOSS2,  MUSIC_BOSS3,
        MUSIC_MIMA,   MUSIC_END1,   MUSIC_ENDING, MUSIC_STAGE5, MUSIC_BOSS5,
    };
    if (request->midi || request->track >= MUSIC_ROOM_TRACK_COUNT) {
        return;
    }
    if (request->action == MUSIC_ROOM_LOAD) {
        M_LoadMusic(songs[request->track]);
    } else {
        M_StartMusic();
    }
}

static void M_OptionsMusic(
    void *const context, const OPTIONS_MENU_SOUND_EVENT *const event)
{
    if (event->action == OPTIONS_MENU_SOUND_STOP) {
        M_StopMusic();
    } else if (
        event->action == OPTIONS_MENU_SOUND_PLAY && !event->midi_active) {
        M_StartMusic();
    }
}

void ProgramAudio_Story(void *const context, const STORY_MUSIC song)
{
    M_LoadMusic(song == STORY_MUSIC_INTRO ? MUSIC_END1 : MUSIC_ENDING);
    M_StartMusic();
}

static void M_StageMusic(void *const context)
{
    if (p_Program->audio.sound.active) {
        M_StopMusic();
    }
    M_LoadMusic((MUSIC_ID)(MUSIC_STAGE0 + p_Program->scenes.session.stage));
    M_StartMusic();
}

static void M_BossMusic(void *const context)
{
    static const MUSIC_ID songs[] = {
        MUSIC_BOSS1, MUSIC_BOSS4, MUSIC_BOSS2,
        MUSIC_BOSS3, MUSIC_MIMA,  MUSIC_BOSS5,
    };
    if (p_Program->audio.sound.active) {
        M_StopMusic();
    }
    M_LoadMusic(songs[p_Program->scenes.session.stage]);
    M_StartMusic();
}

void ProgramAudio_Fade(const uint8_t speed)
{
    if (p_Program->audio.sound.active) {
        MusicWait_FadeOut(&p_Program->audio.music_wait, speed);
        Pmd_Fade(p_Program->audio.pmd, (int8_t)speed);
    }
}

static void M_MimaMusic(void *const context, const BOSS_MUSIC_REQUEST request)
{
    if (request == BOSS_MUSIC_START) {
        M_BossMusic(context);
    } else if (request == BOSS_MUSIC_STOP) {
        M_StopMusic();
    } else {
        ProgramAudio_Fade(10);
    }
}

void ProgramAudio_SaveSettings(void *const context)
{
    p_Program->scenes.save_settings = true;
}

void ProgramAudio_InitSound(OPTIONS_MENU_SOUND *const sound)
{
    memset(sound, 0, sizeof(*sound));
    sound->fm_available = true;
    sound->fm_possible = g_Th02Settings.music != TH02_MUSIC_OFF;
    sound->active = sound->fm_possible;
    sound->emit = M_OptionsMusic;
}

OPENING_AUDIO ProgramAudio_Opening(void)
{
    return (OPENING_AUDIO) {
        .title = M_TitleMusic,
        .music = M_RoomMusic,
        .save_settings = ProgramAudio_SaveSettings,
    };
}

GAMEPLAY_MUSIC ProgramAudio_Gameplay(const bool demo)
{
    return (GAMEPLAY_MUSIC) {
        .entry = demo ? nullptr : &p_Program->audio.music_wait,
        .stage = M_StageMusic,
        .bosses = { .boss = M_BossMusic, .transition = M_MimaMusic },
    };
}
