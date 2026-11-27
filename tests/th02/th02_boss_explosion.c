#include "tests/th02/th02_boss_explosion.h"

static TEST_BOSS_DOTS *m_Dots;

static void M_ObservedDot(int16_t left, int16_t top, uint8_t size)
{
    if (m_Dots != nullptr) {
        m_Dots->count++;
        m_Dots->valid &= left > 24 && left < 416 && top >= 0 && top < 400
            && g_Grcg->mode == 0xc0 && size == m_Dots->size;
    }
    Draw_Dot(left, top, size);
}

#define Draw_Dot M_ObservedDot
#include "th02/boss/explosion.c"
#undef Draw_Dot

void TestBossExplosion_Observe(TEST_BOSS_DOTS *dots)
{
    m_Dots = dots;
}
