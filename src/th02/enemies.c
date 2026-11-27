#include "th02/enemies.h"

#include "th02/background.h"
#include "th02/bullets.h"
#include "th02/draw.h"
#include "th02/items.h"
#include "th02/player.h"
#include "th02/random.h"
#include "th02/score.h"
#include "th02/shots.h"
#include "th02/sound_effects.h"
#include "th02/sparks.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

static ENEMIES m_Enemies;
const ENEMIES *const g_Enemies = &m_Enemies;

static uint8_t M_Read(const STAGE_SCRIPT *const script, const int32_t offset)
{
    ASSERT(offset >= 0 && offset < STAGE_DATA_SCRIPT_SIZE);
    return script->bytes[offset];
}

static void M_Pellet(
    const ENEMY *const enemy, const STAGE_FORMATION *const formation,
    const uint8_t back, const uint8_t angle)
{
    const ENTITY_POINT *const position = &enemy->position[back];

    Bullets_AddPellets(
        back, Math16_Wrap((int32_t)position->x + formation->bullet_offset_x),
        Math16_Wrap((int32_t)position->y + formation->bullet_offset_y), angle,
        enemy->bullet_pattern, enemy->bullet_speed);
}

static void M_Move(
    ENEMY *const enemy, const uint8_t back, const int16_t x, const int16_t y)
{
    ENTITY_POINT *const position = &enemy->position[back];
    position->x = Math16_Wrap((int32_t)position->x + x);
    position->y = Math16_Wrap((int32_t)position->y + y);
    enemy->velocity_x = (int8_t)(uint8_t)x;
    enemy->velocity_y = (int8_t)(uint8_t)y;
}

static int16_t M_Script(
    ENEMY *const enemy, const STAGE_DATA *const data,
    const PLAYFIELD_PAGES pages, const uint8_t scrolled)
{
    const STAGE_FORMATION *const formation =
        &data->formations[enemy->formation];
    const STAGE_SCRIPT *const script = &data->scripts[formation->script];
    ENTITY_POINT *const position = &enemy->position[pages.back];
    const uint8_t ip = (uint8_t)enemy->script_ip;
    const uint8_t command = M_Read(script, ip);
    int16_t length = 1;
    bool waiting = false;
#define PARAM(offset) M_Read(script, (int32_t)ip + (offset))
    switch (command) {
    case 0x00:
        length = 2;
        waiting = true;
        break;
    case 0x01:
    case 0x08: {
        const int sign = command == 0x08 && enemy->spawned_left == 0 ? -1 : 1;
        M_Move(
            enemy, pages.back, (int8_t)(uint8_t)(PARAM(2) * sign),
            (int8_t)PARAM(3));
        length = 4;
        waiting = true;
        break;
    }
    case 0x0a:
        M_Move(enemy, pages.back, enemy->velocity_x, enemy->velocity_y);
        length = 2;
        waiting = true;
        break;
    case 0xaa: {
        const uint8_t angle = Random_RingU8();
        const MATH16_VECTOR vector = Math16_Vector(PARAM(1), angle);
        enemy->velocity_x = (int8_t)(uint8_t)vector.x;
        enemy->velocity_y = (int8_t)(uint8_t)vector.y;
        length = 2;
        break;
    }
    case 0x09: {
        const MATH16_VECTOR vector = Math16_Aim(
            (MATH16_VECTOR) { position->x, position->y },
            (MATH16_VECTOR) {
                Math16_Wrap((int32_t)g_Player->current.x + 24),
                Math16_Wrap((int32_t)g_Player->current.y + 16),
            },
            M_Read(script, (int32_t)enemy->script_ip + 2), 0);
        M_Move(enemy, pages.back, vector.x, vector.y);
        length = 3;
        waiting = true;
        break;
    }
    case 0x02:
        position->y = Math16_Wrap((int32_t)position->y + scrolled);
        length = 2;
        waiting = true;
        break;
    case 0x03:
    case 0x07: {
        const int16_t p =
            Math16_Wrap((int32_t)enemy->script_ip + (command == 3 ? 3 : 2));
        const uint8_t divisor = M_Read(script, (int32_t)p + 1);
        ASSERT(divisor != 0);
        const int sign = command == 3
            ? (M_Read(script, (int32_t)enemy->script_ip + 2) == 0 ? -1 : 1)
            : (enemy->spawned_left == 1 ? -1 : 1);
        if (enemy->wait_frame % divisor == 0) {
            enemy->loop_i = Math16_Wrap((int32_t)enemy->loop_i + sign);
        }
        const uint8_t angle = (uint8_t)(M_Read(script, p) + enemy->loop_i);
        const MATH16_VECTOR vector =
            Math16_Vector(M_Read(script, (int32_t)p + 2), angle);
        M_Move(enemy, pages.back, vector.x, vector.y);
        length = command == 3 ? 6 : 5;
        waiting = true;
        break;
    }
    case 0x0b: {
        const int16_t p = Math16_Wrap((int32_t)enemy->script_ip + 2);
        const int sign = M_Read(script, p) == 0 ? -1 : 1;
        const uint8_t angle =
            (uint8_t)(M_Read(script, (int32_t)p + 1) + enemy->loop_i);
        enemy->loop_i = Math16_Wrap(
            (int32_t)enemy->loop_i + M_Read(script, (int32_t)p + 2) * sign);
        const MATH16_VECTOR vector =
            Math16_Vector(M_Read(script, (int32_t)p + 3), angle);
        M_Move(enemy, pages.back, vector.x, vector.y);
        length = 6;
        waiting = true;
        break;
    }
    case 0x04:
        *position = (ENTITY_POINT) { PARAM(1) * 8, PARAM(2) * 8 };
        length = 3;
        break;
    case 0x05:
    case 0x06: {
        const uint16_t divisor = PARAM(2) * 8;
        ASSERT(divisor != 0);
        const int16_t random_position =
            Math16_Wrap(Random_RingU16() % divisor + PARAM(1) * 8);
        if (command == 5) {
            *position = (ENTITY_POINT) { random_position, PARAM(3) * 8 };
            enemy->spawned_left = position->x < 208 ? 1 : 0;
        } else {
            *position = (ENTITY_POINT) { PARAM(3) * 8, random_position };
        }
        length = 4;
        break;
    }
    case 0x10:
        enemy->state = ENTITY_REMOVE;
        return 2;
    case 0x11:
        enemy->wait_frame = 0;
        SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        enemy->dying = 1;
        return 2;
    case 0x20:
    case 0x21:
    case 0x22:
        M_Pellet(
            enemy, formation, pages.back,
            command == 0x20       ? 0
                : command == 0x21 ? PARAM(1)
                                  : (uint8_t)enemy->loop_i);
        if (command == 0x22) {
            enemy->loop_i = Math16_Wrap((int32_t)enemy->loop_i + PARAM(1));
        }
        length = command == 0x20 ? 1 : 2;
        break;
    case 0x25:
        enemy->bullet_pattern = PARAM(1);
        enemy->bullet_speed = PARAM(2);
        length = 3;
        break;
    case 0x24:
    case 0x26:
    case 0x27: {
        const int16_t left =
            Math16_Wrap((int32_t)position->x + formation->bullet_offset_x);
        int16_t top =
            Math16_Wrap((int32_t)position->y + formation->bullet_offset_y);
        if (command != 0x24) {
            top = Math16_Wrap((int32_t)PARAM(1) - pages.scroll);
            if (top < 0) {
                top = Math16_Wrap((int32_t)top + 400);
            }
        }
        if (command == 0x26) {
            Bullets_AddPellets(
                pages.back, left, top, PARAM(2), PARAM(3), PARAM(4));
            length = 5;
        } else {
            Bullets_AddSprites(
                pages.back, left, top, command == 0x24 ? PARAM(1) : PARAM(2),
                command == 0x24 ? enemy->bullet_pattern : PARAM(3),
                command == 0x24 ? PARAM(2) : PARAM(4),
                command == 0x24 ? enemy->bullet_speed : PARAM(5));
            length = command == 0x24 ? 3 : 6;
        }
        break;
    }
    case 0xa0:
        enemy->script_ip = PARAM(1);
        return 1;
    case 0xa1:
    case 0xa7: {
        int16_t *const counter =
            command == 0xa1 ? &enemy->loop_i : &enemy->loop_j;
        const int16_t previous = *counter;
        *counter = Math16_Wrap((int32_t)*counter + 1);
        if (previous < PARAM(2)) {
            enemy->script_ip = PARAM(1);
        } else {
            enemy->script_ip = Math16_Wrap((int32_t)enemy->script_ip + 3);
            if (command == 0xa7) {
                *counter = 0;
            }
        }
        return 1;
    }
    case 0xa2:
        enemy->loop_i = PARAM(1);
        length = 2;
        break;
    case 0xa3:
    case 0xa4:
        enemy->animation = command == 0xa3 ? 0 : 1;
        break;
    case 0xa5:
    case 0xa6:
        enemy->image = PARAM(1);
        enemy->animation = command == 0xa5 ? 0 : 2;
        length = 2;
        break;
    case 0xa9:
        enemy->clip_y = 1;
        break;
    case 0xa8:
        SoundEffects_Play(PARAM(1));
        length = 2;
        break;
    case 0xab:
    case 0xac:
        enemy->hit_disabled = command == 0xab ? 1 : 0;
        break;
    case 0xad:
    case 0xae:
        enemy->collision_disabled = command == 0xad ? 1 : 0;
        break;
    case 0xaf:
        Background_SetTile(
            pages.front, pages.back, position->x, PARAM(1), PARAM(2));
        length = 3;
        break;
    default:
        ASSERT(false);
        return 2;
    }
#undef PARAM
    if (waiting) {
        enemy->wait_frame = Math16_Wrap((int32_t)enemy->wait_frame + 1);
        if (M_Read(script, (int32_t)ip + 1) <= enemy->wait_frame) {
            enemy->script_ip = Math16_Wrap((int32_t)enemy->script_ip + length);
            enemy->wait_frame = 0;
        }
        return 0;
    }
    enemy->script_ip = Math16_Wrap((int32_t)enemy->script_ip + length);
    return 1;
}

static const PATTERN *M_EnemyPattern(
    const PATTERN_BANK *const patterns, const int16_t image)
{
    ASSERT(image >= 0 && image < PATTERN_LIMIT);
    const PATTERN *const pattern = &patterns->entries[image];
    ASSERT(pattern->data != nullptr);
    return pattern;
}

static void M_Dying(
    ENEMY *const enemy, const STAGE_FORMATION *const formation,
    const PATTERN_BANK *const patterns, const PLAYFIELD_PAGES pages)
{
    const ENTITY_POINT *const position = &enemy->position[pages.back];
    const int16_t image = Math16_Wrap(
        (formation->kill_pattern_offset == 0 ? 10 : 0) + enemy->wait_frame / 3);
    if (enemy->wait_frame == 0) {
        Sparks_Add(
            pages.back, Math16_Wrap((int32_t)position->x + 16),
            (uint16_t)Math16_Wrap((int32_t)position->y + 16), 20, 8, false);
    }
    enemy->wait_frame = Math16_Wrap((int32_t)enemy->wait_frame + 1);
    if (enemy->wait_frame >= 24) {
        enemy->state = ENTITY_REMOVE;
        return;
    }
    int16_t top = Math16_Wrap((int32_t)position->y + pages.scroll);
    if (top >= 400) {
        top -= 400;
    }
    Draw_SpriteRoll(M_EnemyPattern(patterns, image), position->x, top);
}

static int16_t M_EnemyHit(
    ENEMY *const enemy, const STAGE_FORMATION *const formation,
    const uint8_t front, const SHOT_HIT_INPUT input)
{
    const ENTITY_POINT *const position = &enemy->position[input.back];
    const int16_t left = position->x;
    const int16_t top = position->y;
    const int16_t player_left = g_Player->position[front].x;
    if (formation->player_collision_disabled == 0
        && enemy->collision_disabled == 0
        && player_left > Math16_Wrap((int32_t)left - 16)
        && player_left < Math16_Wrap((int32_t)left + 16)
        && g_Player->current.y > Math16_Wrap((int32_t)top - 16)
        && g_Player->current.y < Math16_Wrap((int32_t)top + 16)) {
        enemy->wait_frame = 0;
        Player_Hit();
        enemy->dying = 1;
    }
    const int16_t damage = Shots_Hit(input, left, top, 32, 32);
    Score_AddDamage(damage);
    return damage;
}

void Enemies_Init(void)
{
    memset(&m_Enemies, 0, sizeof(m_Enemies));
}

void Enemies_ResetStage(void)
{
    for (size_t i = 0; i < ENEMY_COUNT; i++) {
        m_Enemies.slots[i].state = ENTITY_FREE;
    }
    m_Enemies.line = 0;
}

void Enemies_Add(
    const uint8_t back, const int16_t left, const int16_t top,
    const uint8_t slot, const int16_t formation)
{
    ASSERT(back < 2 && slot < ENEMY_COUNT);
    ENEMY *const enemy = &m_Enemies.slots[slot];
    enemy->state = ENTITY_ACTIVE;
    enemy->position[back] = (ENTITY_POINT) { left, top };
    enemy->image = enemy->script_ip = enemy->wait_frame = 0;
    enemy->formation = formation;
    enemy->age = Random_RingU8();
    enemy->loop_i = enemy->loop_j = 0;
    enemy->hit_disabled = enemy->collision_disabled = 0;
    enemy->spawned_left = left < 208 ? 1 : 0;
    enemy->clip_y = 0;
    enemy->animation = 1;
    enemy->dying = 0;
    enemy->damage = 0;
    if (m_Enemies.count < slot) {
        m_Enemies.count = slot + 1;
    }
}

void Enemies_Spawn(
    const STAGE_DATA *const data, const uint8_t back, const int16_t rows,
    const uint8_t midboss_active)
{
    if (m_Enemies.line >= data->declared_line_count
        || m_Enemies.line >= data->line_count) {
        return;
    }
    ASSERT(m_Enemies.line >= 0);
    const STAGE_LINE *const line = &data->lines[m_Enemies.line];
    if (line->position != rows) {
        return;
    }
    for (size_t column = 0; column < STAGE_DATA_COLUMNS; column++) {
        const int16_t formation = line->formations[column];
        if (formation == -1 || midboss_active != 0) {
            continue;
        }
        ASSERT(formation >= 0 && formation < data->formation_count);
        for (uint8_t slot = 0; slot < ENEMY_COUNT; slot++) {
            if (m_Enemies.slots[slot].state == ENTITY_FREE) {
                Enemies_Add(
                    back, (int16_t)((column + 1) * 8 + 16),
                    data->formations[formation].spawn_top, slot, formation);
                break;
            }
        }
    }
    m_Enemies.line = Math16_Wrap((int32_t)m_Enemies.line + 1);
}

void Enemies_Invalidate(const PLAYFIELD_PAGES pages)
{
    ASSERT(pages.back < 2 && pages.front < 2);
    m_Enemies.count = 0;
    for (size_t i = 0; i < ENEMY_COUNT; i++) {
        ENEMY *const enemy = &m_Enemies.slots[i];
        if (enemy->state == ENTITY_FREE) {
            continue;
        }
        Background_Mark(
            pages.scroll, enemy->position[pages.back].x,
            enemy->position[pages.back].y, 32, 32);
        if (enemy->state == ENTITY_REMOVE) {
            enemy->state = ENTITY_FREE;
            continue;
        }
        m_Enemies.count = (uint8_t)(i + 1);
        enemy->position[pages.back] = enemy->position[pages.front];
    }
}

void Enemies_RemoveAll(void)
{
    for (size_t i = 0; i < ENEMY_COUNT; i++) {
        if (m_Enemies.slots[i].state != ENTITY_FREE) {
            m_Enemies.slots[i].state = ENTITY_REMOVE;
        }
    }
    m_Enemies.count = 0;
}

void Enemies_Update(
    const PLAYFIELD_PAGES pages, const uint8_t scrolled,
    const STAGE_DATA *const data, const PATTERN_BANK *const patterns,
    const uint32_t frame, const uint8_t rank, const uint8_t bomb_frame_mask)
{
    ASSERT(m_Enemies.count <= ENEMY_COUNT && pages.back < 2 && pages.front < 2);
    const SHOT_HIT_INPUT input = {
        .frame = frame,
        .back = pages.back,
        .bomb_frame_mask = bomb_frame_mask,
    };
    int16_t script_result = 0;
    int16_t target_top = 0;
    Shots_ResetTarget();
    for (size_t i = 0; i < m_Enemies.count; i++) {
        ENEMY *const enemy = &m_Enemies.slots[i];
        if (enemy->state != ENTITY_ACTIVE) {
            continue;
        }
        ASSERT(
            enemy->formation >= 0 && enemy->formation < data->formation_count);
        const STAGE_FORMATION *const formation =
            &data->formations[enemy->formation];
        ASSERT(formation->script < data->script_count);
        ENTITY_POINT *const position = &enemy->position[pages.back];
        do {
            if (enemy->dying == 0) {
                script_result = M_Script(enemy, data, pages, scrolled);
            }
            if (script_result == 2) {
                break;
            }
        } while (script_result == 1);
        if (script_result == 2) {
            continue;
        }
        const int16_t left = position->x;
        int16_t top = position->y;
        if (left < 0 || left > 418
            || (enemy->clip_y != 0 && (top >= 384 || top <= -16))) {
            enemy->state = ENTITY_REMOVE;
            continue;
        }
        enemy->age++;
        if (formation->image_count != 0 && enemy->animation == 1) {
            ASSERT(formation->frames_per_image != 0);
            if (enemy->age % formation->frames_per_image == 0) {
                enemy->image = Math16_Wrap((int32_t)enemy->image + 1);
                if (enemy->image >= formation->image_count) {
                    enemy->image = 0;
                }
            }
        }
        bool render = true;
        if (formation->hp != -1 && enemy->hit_disabled == 0) {
            if (enemy->dying != 0) {
                M_Dying(enemy, formation, patterns, pages);
                continue;
            }
            if (top < 360 && top > target_top) {
                Shots_SetTarget(
                    (ENTITY_POINT) { Math16_Wrap((int32_t)left + 8),
                                     Math16_Wrap((int32_t)top + 8) });
                target_top = top;
            }
            const int16_t damage =
                M_EnemyHit(enemy, formation, pages.front, input);
            if (damage != 0) {
                if (formation->hp == -2) {
                    SoundEffects_Play(SOUND_EFFECT_HIT);
                } else {
                    render = false;
                    enemy->damage =
                        Math16_Wrap((int32_t)enemy->damage + damage);
                    if (enemy->damage >= formation->hp) {
                        enemy->wait_frame = 0;
                        const int16_t item_left =
                            Math16_Wrap((int32_t)left + 12);
                        if (formation->item > 1) {
                            Items_Add(
                                item_left, top, formation->item - ITEM_BOMB);
                        } else {
                            Items_AddRandom(item_left, top);
                        }
                        SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
                        Score_AddPoints((uint32_t)formation->score);
                        if (rank == 3) {
                            Bullets_AddPellets(
                                pages.back,
                                Math16_Wrap(
                                    (int32_t)left + formation->bullet_offset_x),
                                Math16_Wrap(
                                    (int32_t)top + formation->bullet_offset_y),
                                0, BULLET_AIMED, 60);
                        }
                        enemy->dying = 1;
                    } else {
                        SoundEffects_Play(SOUND_EFFECT_HIT);
                        top = Math16_Wrap((int32_t)top + pages.scroll);
                        if (top >= 400) {
                            top -= 400;
                        }
                        if (top < 0) {
                            top = Math16_Wrap((int32_t)top + 400);
                        }
                        Draw_SpriteRollPlane(
                            M_EnemyPattern(
                                patterns,
                                Math16_Wrap(
                                    (int32_t)formation->pattern
                                    + enemy->image)),
                            left, top, 0, 0xffc0);
                    }
                }
            }
        }
        if (!render) {
            continue;
        }
        const int16_t interval = StageData_AimedInterval(formation, rank);
        if (interval > 1 && enemy->age % interval == 0) {
            M_Pellet(enemy, formation, pages.back, 0);
        }
        if (formation->pattern != 0) {
            top = Math16_Wrap((int32_t)position->y + pages.scroll);
            if (top >= 400) {
                top -= 400;
            }
            if (top < 0) {
                top = Math16_Wrap((int32_t)top + 400);
            }
            Draw_SpriteRoll(
                M_EnemyPattern(
                    patterns,
                    enemy->animation != 2
                        ? Math16_Wrap(
                              (int32_t)formation->pattern + enemy->image)
                        : enemy->image),
                left, top);
        }
    }
}
