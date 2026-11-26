#include "th02/music/wait.h"

#include "core/base/assert.h"

#include <string.h>

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
