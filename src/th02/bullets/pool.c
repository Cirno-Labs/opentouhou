#include "th02/bullets/private.h"
#include "th02/background.h"
#include "th02/draw.h"
#include "th02/grcg.h"
#include "th02/player.h"
#include "th02/random.h"
#include "th02/sparks.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"
#include "pc98/zun/sprite_sheet.h"

#include <string.h>

static BULLETS m_Bullets;
const BULLETS *const g_Bullets = &m_Bullets;
BULLETS *const p_Bullets = &m_Bullets;

static void M_Chase(
    BULLET *const bullet, const MATH16_VECTOR from, const MATH16_VECTOR target,
    const int16_t speed)
{
    bullet->velocity.x = Math16_Wrap(
        (int32_t)bullet->velocity.x
        + (from.x < target.x       ? speed
               : from.x > target.x ? -speed
                                   : 0));
    bullet->velocity.y = Math16_Wrap(
        (int32_t)bullet->velocity.y
        + (from.y < target.y       ? speed
               : from.y > target.y ? -speed
                                   : 0));
}

static void M_Special(
    BULLET *const bullet, const uint8_t back, const uint32_t frame)
{
    const MATH16_VECTOR from = {
        Math16_ToPixel(bullet->position[back].x),
        Math16_ToPixel(bullet->position[back].y),
    };
    const MATH16_VECTOR target = {
        Math16_Wrap((int32_t)g_Player->current.x + 8),
        Math16_Wrap((int32_t)g_Player->current.y + 8),
    };
    MATH16_VECTOR velocity;
    switch (bullet->motion) {
    case BULLET_MOTION_CHASE:
        M_Chase(bullet, from, target, m_Bullets.motion_a);
        break;
    case BULLET_MOTION_HOMING:
        bullet->phase++;
        if (bullet->phase < m_Bullets.motion_b) {
            velocity = Math16_Aim(from, target, bullet->speed, 0);
            bullet->velocity = (ENTITY_POINT) { velocity.x, velocity.y };
        } else {
            bullet->motion = 0;
        }
        break;
    case BULLET_MOTION_SLOW_THEN_AIM:
        if ((frame & 7) != 0) {
            break;
        }
        bullet->velocity.x /= 2;
        bullet->velocity.y /= 2;
        if (bullet->velocity.x == 0 && bullet->velocity.y == 0) {
            velocity = Math16_Aim(from, target, bullet->speed, 0);
            bullet->velocity = (ENTITY_POINT) { velocity.x, velocity.y };
            bullet->phase++;
            if (bullet->phase >= m_Bullets.turn_limit) {
                bullet->motion = 0;
            }
        }
        break;
    case BULLET_MOTION_GRAVITY:
        bullet->velocity.y =
            Math16_Wrap((int32_t)bullet->velocity.y + m_Bullets.motion_a);
        break;
    case BULLET_MOTION_DRIFT:
    case BULLET_MOTION_DRIFT_CHASE:
        bullet->angle = (uint8_t)(bullet->angle + m_Bullets.motion_a);
        if (bullet->motion == BULLET_MOTION_DRIFT) {
            bullet->speed = (uint8_t)(bullet->speed + m_Bullets.motion_b);
        }
        velocity = Math16_Vector(bullet->speed, bullet->angle);
        bullet->velocity = (ENTITY_POINT) { velocity.x, velocity.y };
        if (bullet->motion == BULLET_MOTION_DRIFT_CHASE) {
            M_Chase(bullet, from, target, m_Bullets.motion_b);
        }
        bullet->phase++;
        if (bullet->phase > m_Bullets.turn_limit) {
            bullet->motion = 0;
        }
        break;
    case BULLET_MOTION_BOUNCE:
    case BULLET_MOTION_BOUNCE_VERTICAL:
        if (bullet->motion == BULLET_MOTION_BOUNCE
            && (from.x < 40 || from.x > 392)) {
            bullet->velocity.x = Math16_Wrap(-(int32_t)bullet->velocity.x);
            bullet->phase++;
        }
        if (from.y < 24 || from.y > 360) {
            bullet->velocity.y = Math16_Wrap(-(int32_t)bullet->velocity.y);
            bullet->phase++;
        }
        if (bullet->phase > m_Bullets.turn_limit) {
            bullet->motion = 0;
        }
        break;
    default:
        break;
    }
}

static void M_Pellet(
    const BULLET_IMAGES *const images, const int16_t left, const int16_t top)
{
    const uint16_t scaled = (uint16_t)((uint16_t)top << 6);
    uint16_t offset = (uint16_t)(left / 8 + scaled + (scaled >> 2));
    for (size_t row = 0; row < 8; row++) {
        const uint16_t bits = images->rows[(uint16_t)left & 7][row];
        Draw_MaskByte(offset, (uint8_t)bits, 15);
        Draw_MaskByte((uint32_t)offset + 1, (uint8_t)(bits >> 8), 15);
        offset = (uint16_t)(offset + 80);
        if ((int16_t)offset >= 32000) {
            offset -= 32000;
        }
    }
}

static RESULT M_Fill(
    BULLET_IMAGES *const out, const uint8_t *const pellet,
    const size_t pellet_size)
{
    FAIL_IF(pellet_size != 8, "Invalid pellet.bmp size");
    for (size_t shift = 0; shift < 8; shift++) {
        for (size_t row = 0; row < 8; row++) {
            out->rows[shift][row] =
                (uint16_t)((pellet[row] >> shift)
                           | ((unsigned)pellet[row] << (16 - shift)));
        }
    }
    return OK;
}

RESULT BulletImages_Load(VFS *const files, BULLET_IMAGES *const out)
{
    memset(out, 0, sizeof(*out));
    uint8_t *pellet;
    size_t pellet_size;
    MUST(SpriteSheet_Load(
        files, "pellet.bmp", 8, 8, &g_HeapAllocator, &pellet, &pellet_size));
    const RESULT result = M_Fill(out, pellet, pellet_size);
    Mem_Free(pellet);
    return result;
}

void Bullets_Init(void)
{
    memset(&m_Bullets, 0, sizeof(m_Bullets));
    BulletSpawn_Clear();
}

void Bullets_ResetStage(const int8_t rank, const bool reduce_effects)
{
    Sparks_ResetStage(reduce_effects);
    for (size_t i = 0; i < BULLET_COUNT; i++) {
        m_Bullets.slots[i].state = ENTITY_FREE;
    }
    m_Bullets.motion_a = 1;
    m_Bullets.motion_b = 64;
    m_Bullets.turn_limit = 1;
    BulletSpawn_Init(rank);
}

static void M_Add(
    const uint8_t back, const int16_t left, const int16_t top,
    const uint8_t angle, const uint8_t pattern, const int16_t image,
    int16_t speed, const bool sprite)
{
    ASSERT(back < 2);
    if (BulletSpawn_Skip(
            left, top, sprite && pattern == BULLET_MOTION_GRAVITY, &speed)) {
        return;
    }
    size_t slot = 0;
    int16_t index = 0;
    int16_t layer = 0;
    do {
        while (slot < BULLET_COUNT) {
            BULLET *const bullet = &m_Bullets.slots[slot++];
            if (bullet->state != ENTITY_FREE) {
                continue;
            }
            bullet->state = ENTITY_ACTIVE;
            bullet->kind = sprite ? BULLET_SPRITE : BULLET_PELLET;
            bullet->motion = sprite ? pattern : 0;
            bullet->phase = 0;
            bullet->angle = angle;
            if (sprite) {
                bullet->sprite = (uint8_t)image;
                bullet->speed = (uint8_t)speed;
            }
            bool last = true;
            MATH16_VECTOR velocity;
            if (!sprite || pattern < BULLET_MOTION_CHASE) {
                const BULLET_DIRECTION direction =
                    BulletSpawn_Direction(pattern, index, angle, speed);
                index = direction.next_index;
                last = direction.last;
                if (direction.aimed) {
                    const MATH16_VECTOR target = {
                        Math16_Wrap((int32_t)g_Player->current.x + 12),
                        Math16_Wrap((int32_t)g_Player->current.y + 20),
                    };
                    velocity = Math16_Aim(
                        (MATH16_VECTOR) { left, top }, target, direction.speed,
                        direction.angle);
                } else {
                    velocity = Math16_Vector(direction.speed, direction.angle);
                }
            } else {
                velocity = Math16_Vector(speed, angle);
            }
            bullet->position[back] = (ENTITY_POINT) { Math16_FromPixel(left),
                                                      Math16_FromPixel(top) };
            bullet->velocity = (ENTITY_POINT) { velocity.x, velocity.y };
            if (last) {
                break;
            }
        }
    } while (BulletSpawn_NextLayer(&layer, &index, &speed));
}

void Bullets_AddPellets(
    const uint8_t back, const int16_t left, const int16_t top,
    const uint8_t angle, const uint8_t pattern, const int16_t speed)
{
    M_Add(back, left, top, angle, pattern, 0, speed, false);
}

void Bullets_AddSprites(
    const uint8_t back, const int16_t left, const int16_t top,
    const uint8_t angle, const uint8_t motion, const int16_t image,
    const int16_t speed)
{
    M_Add(back, left, top, angle, motion, image, speed, true);
}

void Bullets_Invalidate(const PLAYFIELD_PAGES pages)
{
    ASSERT(pages.back < 2 && pages.front < 2);
    for (size_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &m_Bullets.slots[i];
        if (bullet->state == ENTITY_FREE) {
            continue;
        }
        const int16_t size = bullet->kind * 8;
        Background_Mark(
            pages.scroll, Math16_ToPixel(bullet->position[pages.back].x),
            Math16_ToPixel(bullet->position[pages.back].y), size, size);
        if (bullet->state == ENTITY_REMOVE) {
            bullet->state = ENTITY_FREE;
        } else {
            bullet->position[pages.back] = bullet->position[pages.front];
        }
    }
}

void Bullets_RemoveAll(const PLAYFIELD_PAGES pages)
{
    Bullets_Invalidate(pages);
    for (size_t i = 0; i < BULLET_COUNT; i++) {
        if (m_Bullets.slots[i].state == ENTITY_ACTIVE) {
            m_Bullets.slots[i].state = ENTITY_REMOVE;
        }
    }
}

void Bullets_Update(
    const PLAYFIELD_PAGES pages, const uint32_t frame,
    const PATTERN_BANK *const patterns, const BULLET_IMAGES *const images)
{
    ASSERT(pages.back < 2 && g_Video.drawing_page == pages.back);
    bool charging = false;
    for (size_t i = 0; i < BULLET_COUNT; i++) {
        BULLET *const bullet = &m_Bullets.slots[i];
        if (bullet->state != ENTITY_ACTIVE) {
            continue;
        }
        ENTITY_POINT *const position = &bullet->position[pages.back];
        position->x = Math16_Wrap((int32_t)position->x + bullet->velocity.x);
        position->y = Math16_Wrap((int32_t)position->y + bullet->velocity.y);
        if (bullet->kind == BULLET_SPRITE) {
            M_Special(bullet, pages.back, frame);
        }
        const int16_t left = Math16_ToPixel(position->x);
        int16_t top = Math16_ToPixel(position->y);
        if (left <= 20 || left >= 416 || top >= 384 || top <= 0) {
            bullet->state = ENTITY_REMOVE;
            continue;
        }
        const bool pellet = bullet->kind == BULLET_PELLET;
        const int16_t minimum_x =
            Math16_Wrap((int32_t)g_Player->current.x + (pellet ? 7 : -3));
        const int16_t maximum_x =
            Math16_Wrap((int32_t)g_Player->current.x + (pellet ? 17 : 19));
        const int16_t minimum_y =
            Math16_Wrap((int32_t)g_Player->current.y + (pellet ? 12 : 4));
        const int16_t maximum_y =
            Math16_Wrap((int32_t)g_Player->current.y + (pellet ? 22 : 24));
        if (left >= minimum_x && left < maximum_x && top >= minimum_y
            && top <= maximum_y) {
            Player_Hit();
            bullet->state = ENTITY_REMOVE;
            continue;
        }
        top = Math16_Wrap((int32_t)top + pages.scroll);
        if (top >= 400) {
            top -= 400;
        }
        if (pellet) {
            if (!charging) {
                Grcg_SetColor(15);
                charging = true;
            }
            M_Pellet(images, left, top);
        } else {
            Grcg_Off();
            const PATTERN *const pattern = &patterns->entries[bullet->sprite];
            ASSERT(pattern->data != nullptr);
            Draw_SpriteRollTiny(pattern, left, top);
            charging = false;
        }
    }
    Grcg_Off();
}

void Bullets_SetMotion(const int16_t motion_a, const int16_t motion_b)
{
    m_Bullets.motion_a = motion_a;
    m_Bullets.motion_b = motion_b;
}

void Bullets_SetTurnLimit(const int16_t turn_limit)
{
    m_Bullets.turn_limit = turn_limit;
}

void Bullets_SetSpawnLayers(const uint8_t multiplier)
{
    BulletSpawn_Layers(multiplier);
}
