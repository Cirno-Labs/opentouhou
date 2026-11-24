#include "th01/sound.h"

#include "th01/session.h"
#include "th01/music.h"

static size_t m_Song;

void Sound_Effect(const int16_t number)
{
    if (g_Session.settings.music != SETTINGS_MUSIC_OFF) {
        Music_Effect(number);
    }
}

void Sound_Load(const size_t song)
{
    m_Song = song;
}

void Sound_Play(void)
{
    if (g_Session.settings.music != SETTINGS_MUSIC_OFF) {
        Music_Play(m_Song);
    }
}

void Sound_Fade(void)
{
    if (g_Session.settings.music != SETTINGS_MUSIC_OFF) {
        Music_Fade();
    }
}

void Sound_Stop(void)
{
    if (g_Session.settings.music != SETTINGS_MUSIC_OFF) {
        Music_Stop();
    }
}
