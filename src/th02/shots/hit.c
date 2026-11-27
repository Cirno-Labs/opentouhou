#include "th02/shots/hit.h"

#include "th02/bomb.h"
#include "th02/player.h"
#include "th02/shots/pool.h"
#include "th02/shots/private.h"
#include "th02/sparks.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

static const int16_t m_BodyDamage[] = { 5, 5, 4, 4, 3, 3, 3, 2, 2, 1 };
static const int16_t m_OptionDamage[] = { 2, 2, 2, 2, 2, 1, 1, 1, 1, 1 };
static const int16_t m_LargeDamage[] = { 0, 0, 8, 8, 7, 7, 6, 5, 4, 3 };

static int16_t M_Eighth(const int16_t value)
{
    return (int16_t)(value / 8 - (value % 8 < 0));
}

int16_t Shots_Hit(
    const SHOT_HIT_INPUT input, const int16_t left, const int16_t top,
    const int16_t width, const int16_t height)
{
    const uint8_t level = g_Shots->weapon.level;
    const bool bombing = g_Bomb->active != 0;
    ASSERT(level < 10 && input.back < 2);
    const int16_t body_right = Math16_Wrap((int32_t)left + width);
    const int16_t minimum_x = Math16_Wrap((int32_t)left - 8);
    const int16_t minimum_y = Math16_Wrap((int32_t)top - 8);
    const int16_t maximum_y = Math16_Wrap((int32_t)minimum_y + height);
    const int16_t option_right = Math16_Wrap((int32_t)minimum_x + width);
    int16_t damage = 0;
    for (size_t i = 0; i < 35; i++) {
        SHOT *const shot = &p_Shots->slots[i];
        if (shot->state != ENTITY_ACTIVE
            || (shot->from_option == 0 && shot->age != 0)) {
            continue;
        }
        ENTITY_POINT *const position = &shot->position[input.back];
        const int16_t x = Math16_ToPixel(position->x);
        const int16_t y = Math16_ToPixel(position->y);
        const int16_t maximum_x =
            shot->from_option == 0 ? body_right : option_right;
        if (x <= minimum_x || x >= maximum_x || y <= minimum_y
            || y >= maximum_y) {
            continue;
        }
        if (shot->from_option == 0) {
            damage++;
            if (shot->sprite >= 50) {
                shot->velocity.x = M_Eighth(shot->velocity.x);
                shot->velocity.y = M_Eighth(shot->velocity.y);
                if (shot->sprite >= 120) {
                    shot->age = 1;
                    position->x = Math16_Wrap((int32_t)position->x - 128);
                    position->y = Math16_Wrap((int32_t)position->y - 128);
                    damage += m_LargeDamage[level];
                } else {
                    shot->age = 2;
                    damage += m_BodyDamage[level];
                }
            }
            Sparks_Add(
                input.back, x, (uint16_t)Math16_ToPixel(position->y), 30, 1,
                false);
        } else {
            if (shot->age == 1) {
                Sparks_Add(input.back, x, (uint16_t)y, 40, 1, false);
                shot->age = 2;
            }
            damage += m_OptionDamage[level];
        }
    }
    if (bombing && ((uint8_t)input.frame & input.bomb_frame_mask) == 0) {
        damage++;
    }
    return damage;
}
