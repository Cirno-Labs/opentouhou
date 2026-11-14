#include "th07/game.h"
#include "th07/sound.h"

#include "test.h"

#include <string.h>

static void M_CheckQueue(const int32_t *const expected)
{
    for (int32_t i = 0; i < SOUND_QUEUE_SIZE; i++) {
        CHECK_EQ_INT(g_SoundEvents.queue[i], expected[i]);
    }
}

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
