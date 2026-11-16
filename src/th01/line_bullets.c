#include "th01/line_bullets.h"

static void M_HitTest(
    PLAYER *const player, const int16_t right, const int16_t bottom)
{
    if (right > player->left && right < player->left + 32 && bottom > 384
        && !player->invincible) {
        player->is_hit = true;
    }
}

void LineBullets_PutAndHitTest(
    VIDEO *const video, PLAYER *const player, const int16_t left,
    const int16_t top, const int16_t right, const int16_t bottom,
    const uint8_t color)
{
    Video_Line(video, left, top, right, bottom, color);
    M_HitTest(player, right, bottom);
}

void LineBullets_PutPatternedAndHitTest(
    VIDEO *const video, PLAYER *const player, const int16_t left,
    const int16_t top, const int16_t right, const int16_t bottom,
    const uint8_t color, const uint16_t pattern)
{
    Video_LinePatterned(video, left, top, right, bottom, color, pattern);
    M_HitTest(player, right, bottom);
}

void LineBullets_Erase(
    VIDEO *const video, const int16_t left, const int16_t top,
    const int16_t right, const int16_t bottom)
{
    Video_LineErase(video, left, top, right, bottom);
}
