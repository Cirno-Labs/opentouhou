#include "tests/th02/th02_boss_explosion.h"

static TEST_BOSS_DOTS *m_Dots;

static void M_ObservedDot(
    const int16_t left, const int16_t top, const uint8_t size)
{
    if (m_Dots != nullptr) {
        m_Dots->count++;
        m_Dots->valid &= left > 24 && left < 416 && top >= 0 && top < 400
            && g_Grcg->mode == 0xc0 && size == m_Dots->size;
    }
    Draw_Dot(left, top, size);
}

static const BOSS_EXPLOSION_DOT m_TestDot = { .dot = M_ObservedDot };

void TestBossExplosion_Observe(TEST_BOSS_DOTS *const dots)
{
    m_Dots = dots;
    BossExplosion_Init(&m_TestDot);
}
