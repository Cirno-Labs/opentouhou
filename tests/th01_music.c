#include "../src/th01/music.c"
#include "../src/th01/sound.c"
#include "th01_music.h"

MDRV *TestMusic_Driver(void)
{
    return m_Driver;
}

GAME_AUDIO *TestMusic_Output(void)
{
    return m_Output;
}

size_t TestMusic_SelectedSong(void)
{
    return m_Song;
}
