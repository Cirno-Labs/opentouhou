#include "th07/sound/events.h"

#include "th07/game.h"

#include "core/math.h"

#include <string.h>

// The original has 0x80 sound buffers, of which the sound table fills the
// first SOUND_COUNT.
#define M_SLOT_COUNT 0x80

SOUND_EVENTS g_SoundEvents = { .queue = { -1, -1, -1, -1, -1 } };

// Writes path into out with "wav" over the three characters after its last
// dot. A path without a dot, or one that does not fit, gives false.
static bool M_WavePath(const char *const path, char *const out)
{
    const size_t length = strlen(path);
    const char *const dot = strrchr(path, '.');
    const size_t stem = dot != nullptr ? (size_t)(dot + 1 - path) : 0;
    if (dot == nullptr || length + 3 >= SOUND_PATH_SIZE) {
        return false;
    }
    memcpy(out, path, length + 1);
    memcpy(out + stem, "wav", 3);
    if (length < stem + 3) {
        out[stem + 3] = '\0';
    }
    return true;
}

void Sound_Reset(void)
{
    g_SoundEvents = (SOUND_EVENTS) { .queue = { -1, -1, -1, -1, -1 } };
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
    if (M_WavePath(path, g_SoundEvents.music_path)) {
        g_SoundEvents.music = SOUND_MUSIC_PLAY;
        g_SoundEvents.fade_out = false;
        g_SoundEvents.pause = SOUND_PAUSE_KEEP;
    }
}

void Sound_StopMusic(void)
{
    g_SoundEvents.music = SOUND_MUSIC_STOP;
    g_SoundEvents.fade_out = false;
    g_SoundEvents.pause = SOUND_PAUSE_KEEP;
}

void Sound_PauseMusic(void)
{
    g_SoundEvents.pause = SOUND_PAUSE_MUSIC;
}

void Sound_ResumeMusic(void)
{
    g_SoundEvents.pause = SOUND_RESUME_MUSIC;
}

void Sound_FadeOutMusic(const float seconds)
{
    if (g_SoundEvents.music == SOUND_MUSIC_STOP) {
        return;
    }
    float duration = seconds;
    if (g_GameSpeed != 0.0f && g_GameSpeed <= 1.0f) {
        duration = seconds / g_GameSpeed;
    }
    g_SoundEvents.fade_out = true;
    g_SoundEvents.fade_seconds = X87_Ftol(duration);
}
