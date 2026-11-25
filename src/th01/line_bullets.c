#include "th01/line_bullets.h"

static void M_HitTest(const int16_t right, const int16_t bottom)
{
    if (right > g_Player->left && right < g_Player->left + 32 && bottom > 384
        && !g_Player->invincible) {
        Player_Hit();
    }
}

void LineBullets_PutAndHitTest(
    const int16_t left, const int16_t top, const int16_t right,
    const int16_t bottom, const uint8_t color)
{
    Video_Line(left, top, right, bottom, color);
    M_HitTest(right, bottom);
}

void LineBullets_PutPatternedAndHitTest(
    const int16_t left, const int16_t top, const int16_t right,
    const int16_t bottom, const uint8_t color, const uint16_t pattern)
{
    Video_LinePatterned(left, top, right, bottom, color, pattern);
    M_HitTest(right, bottom);
}

void LineBullets_Erase(
    const int16_t left, const int16_t top, const int16_t right,
    const int16_t bottom)
{
    Video_LineErase(left, top, right, bottom);
}
