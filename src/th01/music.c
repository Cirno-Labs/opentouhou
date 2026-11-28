#include "th01/music_private.h"

static MDRV *m_Driver;
static GAME_AUDIO *m_Output;
MDRV **const p_Driver = &m_Driver;
GAME_AUDIO **const p_Output = &m_Output;

RESULT Music_Init(
    const ALLOCATOR *const allocator, const MDRV_MUSIC *const songs,
    const size_t song_count, const MDRV_EFFECTS *const effects)
{
    m_Output = nullptr;
    return Mdrv_Init(songs, song_count, effects, 8, 8, allocator, &m_Driver);
}

void Music_BindOutput(GAME_AUDIO *const output)
{
    m_Output = output;
}

void Music_Update(void)
{
    GameAudio_Render(m_Output, m_Driver, 800);
}

void Music_Play(const size_t song)
{
    Mdrv_Play(m_Driver, song);
}

void Music_Fade(void)
{
    Mdrv_Fade(m_Driver, 1);
}

void Music_Stop(void)
{
    Mdrv_Stop(m_Driver);
}

void Music_Effect(const int16_t number)
{
    Mdrv_Effect(m_Driver, (uint16_t)number);
}

void Music_Close(void)
{
    m_Output = nullptr;
    m_Driver = nullptr;
}
