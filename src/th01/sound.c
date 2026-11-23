#include "th01/sound.h"

#include "th01/session.h"

SOUND g_Sound;

void Sound_Effect(const int16_t number)
{
    if (g_Session.settings.music != SETTINGS_MUSIC_OFF) {
        Mdrv_Effect(g_Sound.mdrv, (uint16_t)number);
    }
}

void Sound_Load(const size_t song)
{
    g_Sound.song = song;
}

void Sound_Play(void)
{
    if (g_Session.settings.music != SETTINGS_MUSIC_OFF) {
        Mdrv_Play(g_Sound.mdrv, g_Sound.song);
    }
}

void Sound_Fade(void)
{
    if (g_Session.settings.music != SETTINGS_MUSIC_OFF) {
        Mdrv_Fade(g_Sound.mdrv, 1);
    }
}

void Sound_Stop(void)
{
    if (g_Session.settings.music != SETTINGS_MUSIC_OFF) {
        Mdrv_Stop(g_Sound.mdrv);
    }
}
