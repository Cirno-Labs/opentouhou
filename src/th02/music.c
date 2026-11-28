#include "th02/music.h"

#include "core/base/assert.h"

#include <string.h>

static const char *const m_Paths[MUSIC_SONG_COUNT] = {
    "stage0.m", "stage1.m", "stage2.m", "stage3.m", "stage4.m",
    "stage5.m", "boss1.m",  "boss2.m",  "boss3.m",  "boss4.m",
    "boss5.m",  "mima.m",   "op.m",     "end1.m",   "ending.m",
};

const char *MusicAssets_Path(const MUSIC_ID id)
{
    return (uint32_t)id < MUSIC_SONG_COUNT ? m_Paths[id] : nullptr;
}

RESULT MusicAssets_Load(
    VFS *const files, const ALLOCATOR *const allocator, MUSIC_ASSETS *const out)
{
    memset(out, 0, sizeof(*out));
    MUSIC_ASSETS assets = {};
    for (size_t i = 0; i < MUSIC_SONG_COUNT; i++) {
        READER reader;
        RESULT result = Vfs_Open(files, m_Paths[i], VFS_STREAM, &reader);
        if (IS_OK(result)) {
            result = PmdData_LoadSong(&reader, allocator, &assets.songs[i]);
            Reader_Close(&reader);
        }
        if (!IS_OK(result)) {
            return Result_Prefix(result, "%s", m_Paths[i]);
        }
    }
    *out = assets;
    return OK;
}

#define M_TIMER_A_PERIOD 737280
#define M_SAMPLE_PERIOD 832

void MusicWait_Init(MUSIC_WAIT *const wait)
{
    memset(wait, 0, sizeof(*wait));
}

void MusicWait_Start(MUSIC_WAIT *const wait)
{
    wait->timer_a = wait->volume = wait->speed = 0;
}

void MusicWait_Stop(MUSIC_WAIT *const wait)
{
    wait->volume = 255;
    wait->speed = 0;
}

void MusicWait_Advance(MUSIC_WAIT *const wait, const uint32_t frames)
{
    const uint64_t time = wait->phase + (uint64_t)frames * M_SAMPLE_PERIOD;
    uint64_t overflows = time / M_TIMER_A_PERIOD;
    wait->phase = (uint32_t)(time % M_TIMER_A_PERIOD);
    while (overflows-- != 0) {
        wait->timer_a++;
        if ((wait->timer_a & 7) == 0 && wait->speed != 0) {
            const unsigned volume = (unsigned)wait->volume + wait->speed;
            if (volume > 255) {
                wait->volume = 255;
                wait->speed = 0;
            } else {
                wait->volume = (uint8_t)volume;
            }
        }
    }
}

void MusicWait_FadeOut(MUSIC_WAIT *const wait, const uint8_t speed)
{
    ASSERT(speed != 0 && speed < 128);
    wait->speed = speed;
}

bool MusicWait_Ready(const MUSIC_WAIT *const wait)
{
    return wait->volume == 255;
}
