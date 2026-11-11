#include "th06/sound/events.h"

#include <string.h>

// The original has 0x80 sound buffers, of which the sound table fills the
// first SOUND_COUNT.
#define M_SLOT_COUNT 0x80

SOUND_EVENTS g_SoundEvents = { .queue = { -1, -1, -1 } };

void Sound_Reset(void)
{
    g_SoundEvents = (SOUND_EVENTS) { .queue = { -1, -1, -1 } };
}

void Sound_Queue(const int32_t sound)
{
    if (sound < 0 || sound >= M_SLOT_COUNT) {
        return;
    }
    int32_t i = 0;
    while (i < SOUND_QUEUE_SIZE && g_SoundEvents.queue[i] >= 0) {
        if (g_SoundEvents.queue[i] == sound) {
            return;
        }
        i++;
    }
    if (i < SOUND_QUEUE_SIZE) {
        g_SoundEvents.queue[i] = sound;
    }
}

void Sound_PlayMusic(const char *const path)
{
    const size_t length = strlen(path);
    if (length >= SOUND_PATH_SIZE) {
        return;
    }
    memcpy(g_SoundEvents.music_path, path, length + 1);
    g_SoundEvents.music = SOUND_MUSIC_PLAY;
    g_SoundEvents.fade_out = false;
}

void Sound_StopMusic(void)
{
    g_SoundEvents.music = SOUND_MUSIC_STOP;
    g_SoundEvents.fade_out = false;
}

void Sound_FadeOutMusic(const float seconds)
{
    if (g_SoundEvents.music != SOUND_MUSIC_STOP) {
        g_SoundEvents.fade_out = true;
        g_SoundEvents.fade_seconds = seconds;
    }
}
