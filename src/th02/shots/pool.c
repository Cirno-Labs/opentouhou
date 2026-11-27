#include "th02/shots/pool.h"
#include "th02/shots/private.h"
#include "th02/background.h"
#include "th02/draw.h"
#include "th02/shots/fire.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

static SHOTS m_Shots;
const SHOTS *const g_Shots = &m_Shots;
SHOTS *const p_Shots = &m_Shots;

static int16_t M_Halve(const int16_t value)
{
    return value / 2 - (value % 2 < 0);
}

static void M_PutShot(
    const PATTERN_BANK *const patterns, const int16_t left, const int16_t top,
    const uint16_t sprite, const bool tiny)
{
    ASSERT(sprite < PATTERN_LIMIT);
    const PATTERN *const pattern = &patterns->entries[sprite];
    ASSERT(pattern->data != nullptr);
    if (tiny) {
        Draw_SpriteRollTiny(pattern, left, top);
    } else {
        Draw_SpriteRoll(pattern, left, top);
    }
}

void Shots_Init(void)
{
    memset(&m_Shots, 0, sizeof(m_Shots));
    p_Shots->option_frames_per_age = 4;
    p_Shots->weapon.parameter[0] = 64;
    p_Shots->weapon.parameter[1] = 76;
    p_Shots->weapon.cycle[0] = p_Shots->weapon.cycle[1] = 255;
    Shots_ResetTarget();
}

void Shots_FreeAll(void)
{
    for (size_t i = 0; i < SHOT_COUNT; i++) {
        p_Shots->slots[i].state = ENTITY_FREE;
    }
}

void Shots_ResetStage(const uint8_t type)
{
    ASSERT(type < 3);
    Shots_FreeAll();
    p_Shots->option_active = 0;
    if (type == 2) {
        p_Shots->option_frames_per_age = 3;
    }
    static const uint8_t maximums[] = { 48, 52, 125 };
    p_Shots->weapon.spread = 0;
    p_Shots->weapon.maximum[0] = maximums[type];
    if (type != 0) {
        p_Shots->weapon.maximum[1] = type == 1 ? 55 : 59;
    }
    Shots_ResetTarget();
}

void Shots_Invalidate(const PLAYFIELD_PAGES pages, const bool reduce_effects)
{
    ASSERT(pages.back < 2 && pages.front < 2);
    bool option_alive = false;
    for (size_t i = 0; i < SHOT_COUNT; i++) {
        SHOT *const shot = &p_Shots->slots[i];
        if (shot->state == ENTITY_FREE) {
            continue;
        }
        if (!reduce_effects || pages.back == (i & 1)) {
            const int16_t size = shot->sprite < 124 || shot->age == 0 ? 16 : 32;
            Background_Mark(
                pages.scroll, Math16_ToPixel(shot->position[pages.back].x),
                Math16_ToPixel(shot->position[pages.back].y), size, size);
        }
        if (shot->state == ENTITY_REMOVE) {
            shot->state = ENTITY_FREE;
            continue;
        }
        shot->position[pages.back] = shot->position[pages.front];
        option_alive |= shot->from_option != 0;
    }
    if (!option_alive) {
        p_Shots->option_active = 0;
    }
}

void Shots_Update(
    const PLAYFIELD_PAGES pages, const bool reduce_effects,
    const uint32_t frame, const PATTERN_BANK *const patterns)
{
    ASSERT(pages.back < 2);
    static const uint16_t big_hits[] = { 39, 39, 39, 40, 41, 47 };
    uint8_t skip = (uint8_t)reduce_effects << pages.back;
    for (size_t i = 0; i < SHOT_COUNT; i++, skip ^= 3) {
        SHOT *const shot = &p_Shots->slots[i];
        if (shot->state != ENTITY_ACTIVE) {
            continue;
        }
        ENTITY_POINT *const position = &shot->position[pages.back];
        position->x = Math16_Wrap(position->x + (int32_t)shot->velocity.x);
        position->y = Math16_Wrap(position->y + (int32_t)shot->velocity.y);
        int16_t top = Math16_ToPixel(position->y);
        const int16_t left = Math16_ToPixel(position->x);
        if (position->x <= 16 * 16 || position->x >= 418 * 16 || top > 384
            || top < 8) {
            shot->state = ENTITY_REMOVE;
            continue;
        }
        top = Math16_Wrap(top + (int32_t)pages.scroll);
        if (top >= 400) {
            top -= 400;
        }
        if (shot->from_option == 0) {
            p_Shots->frames[i]++;
            if (shot->age == 0) {
                if (skip != 2) {
                    const uint16_t sprite = shot->sprite
                        + (shot->sprite >= 50 ? p_Shots->frames[i] & 1 : 0);
                    M_PutShot(patterns, left, top, sprite, true);
                }
            } else {
                if (shot->age > 5) {
                    shot->state = ENTITY_REMOVE;
                    continue;
                }
                if (skip != 2) {
                    M_PutShot(
                        patterns, left, top,
                        shot->sprite < 124 ? shot->age + 74
                                           : big_hits[shot->age],
                        shot->sprite < 124);
                }
                if ((frame & 3) == 0) {
                    shot->age++;
                }
            }
        } else {
            if (shot->age > 1) {
                ASSERT(p_Shots->option_frames_per_age != 0);
                if (frame % (uint32_t)(int32_t)p_Shots->option_frames_per_age
                    == 0) {
                    shot->age++;
                    if (shot->age > 4) {
                        shot->state = ENTITY_REMOVE;
                        continue;
                    }
                }
                if ((frame & 1) != 0) {
                    shot->velocity.y = M_Halve(shot->velocity.y);
                }
            } else if (shot->sprite != 59) {
                shot->velocity.y = Math16_Wrap(shot->velocity.y - 4);
            }
            if (skip != 2) {
                M_PutShot(patterns, left, top, shot->sprite + shot->age, true);
            }
        }
    }
}
