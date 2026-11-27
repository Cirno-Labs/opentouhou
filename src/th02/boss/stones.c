#include "th02/boss/stones.h"
#include "th02/background.h"
#include "th02/pillars.h"
#include "th02/stage.h"

#include "th02/boss/explosion.h"
#include "th02/boss/private.h"
#include "th02/boss/strategy.h"
#include "th02/bullets.h"
#include "th02/enemies.h"
#include "th02/items.h"
#include "th02/player.h"
#include "th02/shots.h"
#include "th02/random.h"
#include "th02/score.h"
#include "th02/sound_effects.h"
#include "th02/sparks.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

#define STONE_COUNT 5
#define STONE_MIDBOSS_COUNT 2

typedef enum {
    STONE_INNER_WEST,
    STONE_INNER_EAST,
    STONE_OUTER_WEST,
    STONE_OUTER_EAST,
    STONE_NORTH,
} STONE_INDEX;

typedef enum {
    STONE_DORMANT,
    STONE_ACTIVE,
    STONE_KILL,
    STONE_REMOVE,
    STONE_REMOVED,
} STONE_FLAG;

typedef enum {
    STONE_MIDBOSS_ACTIVE,
    STONE_MIDBOSS_KILL,
    STONE_MIDBOSS_REMOVED,
} STONE_MIDBOSS_FLAG;

typedef enum {
    STONES_OPEN_INNER,
    STONES_INNER,
    STONES_OUTER,
    STONES_OPEN_NORTH,
    STONES_NORTH_FIRST,
    STONES_CLOSE_NORTH,
    STONES_NORTH_CLOSED,
    STONES_REOPEN_NORTH,
    STONES_NORTH_FINAL,
} STONES_PHASE;

typedef struct {
    uint8_t flag[STONE_COUNT];
    int16_t damage[STONE_COUNT];
    int16_t sprite[STONE_COUNT];
    ENTITY_POINT midboss_position[STONE_COUNT][2];
    int16_t midboss_kill_frame[STONE_MIDBOSS_COUNT];
    uint8_t midboss_angle[STONE_MIDBOSS_COUNT];
    uint8_t midboss_rounds;
    ENTITY_POINT position[STONE_COUNT];
    ENTITY_POINT bullet;
    int16_t kill_frame[STONE_COUNT];
    int16_t pattern_frame;
    int16_t ring_y;
    int16_t pillar_left;
    int32_t frames;
    STONES_PHASE phase;
    uint8_t flash[STONE_COUNT];
    uint8_t pattern;
    uint8_t pillar_state;
    uint8_t spread;
    uint8_t sprite_start;
    uint8_t ring;
    uint8_t firing_ring;
    uint8_t sweep_angle;
    uint8_t fan_angle;
    uint8_t ring_angle;
    uint8_t aim_angle[STONE_COUNT];
    uint8_t ring_flag[TILE_COLUMNS];
    uint8_t ring_phase;
    uint8_t ring_index;
    uint8_t ring_done;
    int16_t ring_tile[3];
} STONES;

typedef void (*STONE_ATTACK)(BOSS_STATE *boss, uint8_t rank);

static STONES m_Stones;

static const PATTERN *M_Pattern(
    const PATTERN_BANK *const patterns, const int16_t id)
{
    ASSERT(id >= 0 && id < PATTERN_LIMIT);
    const PATTERN *const pattern = &patterns->entries[id];
    ASSERT(pattern->data != nullptr);
    return pattern;
}

static int16_t M_Top(const int16_t top)
{
    int16_t rolled = Math16_Wrap((int32_t)top + g_Stage->scroll.line);
    if (rolled >= 400) {
        rolled -= 400;
    }
    return rolled;
}

static ENTITY_POINT *M_MidbossPosition(const int i)
{
    return &m_Stones.midboss_position[i][g_Stage->scroll.back];
}

static void M_MidbossDraw(
    const PATTERN_BANK *const patterns, const int i, const bool hit)
{
    const ENTITY_POINT position = *M_MidbossPosition(i);
    const int16_t top = M_Top(position.y);
    const PATTERN *const pattern = M_Pattern(patterns, m_Stones.sprite[0]);
    if (hit) {
        SoundEffects_Play(SOUND_EFFECT_HIT);
        Draw_SpriteRollPlane(pattern, position.x, top, 0, 0xffc0);
    } else {
        Draw_SpriteRoll(pattern, position.x, top);
    }
}

static void M_MidbossDefeat(const PATTERN_BANK *const patterns, const int i)
{
    STONES *const stones = &m_Stones;
    const ENTITY_POINT position = *M_MidbossPosition(i);
    const int16_t x = Math16_Wrap((int32_t)position.x + 32);
    const int16_t y = Math16_Wrap((int32_t)position.y + 16);
    Sparks_Add(g_Stage->scroll.back, x, (uint16_t)y, 60, 1, false);
    const int16_t image = 10 + stones->midboss_kill_frame[i] / 6;
    stones->midboss_kill_frame[i] =
        Math16_Wrap((int32_t)stones->midboss_kill_frame[i] + 1);
    if (stones->midboss_kill_frame[i] >= 48) {
        Sparks_Add(g_Stage->scroll.back, x, (uint16_t)y, 128, 32, true);
        stones->midboss_kill_frame[i] = 0;
        stones->flag[i] = STONE_MIDBOSS_REMOVED;
        Shots_ResetTarget();
    } else {
        const int16_t top = M_Top(position.y);
        Draw_SpriteRoll(M_Pattern(patterns, image), position.x, top);
        Draw_SpriteRoll(
            M_Pattern(patterns, image), Math16_Wrap((int32_t)position.x + 32),
            top);
    }
}

static void M_MidbossFire(
    BOSS_STATE *const boss, const uint8_t rank, const int i)
{
    STONES *const stones = &m_Stones;
    ENTITY_POINT *const position = M_MidbossPosition(i);
    const int16_t velocity = i == 0 ? 8 : -8;
    const int16_t top = Math16_Wrap((int32_t)position->y + 12);
    const int16_t frame = boss->phase_frame;
    if (frame < 152) {
        if (frame == 128) {
            stones->midboss_angle[0] = 0;
            stones->midboss_angle[1] = 128;
        }
        if ((frame & 3) == 0) {
            stones->midboss_angle[i] =
                (uint8_t)(stones->midboss_angle[i] + velocity);
            Bullets_AddPellets(
                g_Stage->scroll.back, Math16_Wrap((int32_t)position->x + 28),
                top, stones->midboss_angle[i], BULLET_SINGLE, 40);
        }
    } else if (frame < 192) {
        position->x = Math16_Wrap((int32_t)position->x + velocity);
        if ((frame & 7) == 0) {
            stones->midboss_angle[i] =
                (uint8_t)(stones->midboss_angle[i] + velocity * 2);
            Bullets_AddPellets(
                g_Stage->scroll.back, Math16_Wrap((int32_t)position->x + 28),
                top, (uint8_t)(128 - stones->midboss_angle[i]),
                rank != 0 ? BULLET_FAN4_MEDIUM : BULLET_FAN2_MEDIUM, 40);
        }
    } else if (frame < 352) {
        position->y = Math16_Wrap((int32_t)position->y + 2);
        if (position->y >= 400) {
            position->y -= 400;
        }
    } else if (frame < 376) {
        if ((frame & 3) == 0) {
            stones->midboss_angle[i] =
                (uint8_t)(stones->midboss_angle[i] - velocity);
            Bullets_AddPellets(
                g_Stage->scroll.back, Math16_Wrap((int32_t)position->x + 28),
                top, stones->midboss_angle[i], BULLET_SINGLE, 50);
        }
    } else if (frame < 416) {
        position->x = Math16_Wrap((int32_t)position->x - velocity);
        if ((frame & 3) == 0) {
            stones->midboss_angle[i] =
                (uint8_t)(stones->midboss_angle[i] - velocity);
            Bullets_AddPellets(
                g_Stage->scroll.back, Math16_Wrap((int32_t)position->x + 28),
                top, stones->midboss_angle[i], BULLET_SINGLE, 50);
        }
    } else if (frame < 576) {
        position->y = Math16_Wrap((int32_t)position->y - 2);
        if (position->y < 0) {
            position->y = Math16_Wrap((int32_t)position->y + 400);
        }
        if (frame % 60 == 0) {
            Bullets_AddPellets(
                g_Stage->scroll.back, Math16_Wrap((int32_t)position->x + 28),
                top, 0, BULLET_AIMED, 40);
        }
    } else {
        boss->phase_frame = 127;
    }
}

static int16_t M_MidbossHit(const int i)
{
    const ENTITY_POINT position = *M_MidbossPosition(i);
    return Boss_Hit(
        &m_Stones.damage[i], Math16_Wrap((int32_t)position.x + 12), position.y,
        40, 30, 1, 1);
}

static void M_RollPut(
    const PATTERN_BANK *const patterns, const int16_t left, const int16_t top,
    const int16_t sprite)
{
    Draw_SpriteRoll(M_Pattern(patterns, sprite), left, top);
}

static void M_Put(
    const PATTERN_BANK *const patterns, const int16_t left, const int16_t top,
    const int16_t sprite)
{
    Draw_Sprite(M_Pattern(patterns, sprite), left, top);
}

static void M_Plane(
    const PATTERN_BANK *const patterns, const int16_t left, const int16_t top,
    const int16_t sprite, const uint8_t plane, const uint16_t mode)
{
    Draw_SpriteRollPlane(M_Pattern(patterns, sprite), left, top, plane, mode);
}

static int16_t M_Hit(
    const int16_t left, const int16_t top, const int16_t width,
    const int16_t height)
{
    int16_t ignored = 0;
    return Boss_Hit(&ignored, left, top, width, height, 0, 1);
}

static void M_OuterSpread(void)
{
    const STONES *const stones = &m_Stones;
    for (int stone = STONE_OUTER_WEST; stone < STONE_NORTH; stone++) {
        if (stones->flag[stone] == STONE_ACTIVE) {
            for (int i = 0; i < 6; i++) {
                Bullets_AddSprites(
                    g_Stage->scroll.back, stones->position[stone].x + 12,
                    stones->position[stone].y + 8, (uint8_t)(i * 42),
                    BULLET_MOTION_SLOW_THEN_AIM, 82, 56);
            }
        }
    }
}

static void M_Draw(const PATTERN_BANK *const patterns)
{
    STONES *const stones = &m_Stones;
    int i;
    int16_t vram_top;

    for (i = 0; i < STONE_COUNT; i++) {
        if (stones->flag[i] < STONE_KILL) {
            vram_top = stones->position[i].y;
            vram_top += g_Stage->scroll.line;
            if (vram_top >= 400) {
                vram_top -= 400;
            }
            if (stones->flash[i] != 0) {
                SoundEffects_Play(SOUND_EFFECT_HIT);
                M_Plane(
                    patterns, stones->position[i].x, vram_top,
                    stones->sprite[i], 0, 0xffc0);
                stones->flash[i] = 0;
            } else {
                M_RollPut(
                    patterns, stones->position[i].x, vram_top,
                    stones->sprite[i]);
            }
        }
    }
}

static int M_Defeat(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns, int stone)
{
    STONES *const stones = &m_Stones;
    int patnum;
    int frames;
    int16_t vram_top;

    frames = 64;
    Sparks_Add(
        g_Stage->scroll.back, (stones->position[stone].x + 16),
        (stones->position[stone].y + 12), (5 << 4), 2, false);
    patnum = 10;
    vram_top = stones->position[stone].y;
    vram_top += g_Stage->scroll.line;
    if (vram_top >= 400) {
        vram_top -= 400;
    }
    if (stone == STONE_NORTH) {
        frames += 32;
        BossExplosion_Draw(
            patterns, boss->angle, (stones->position[STONE_NORTH].x + 8),
            (stones->position[STONE_NORTH].y + 8),
            stones->kill_frame[STONE_NORTH]);
        if (stones->kill_frame[STONE_NORTH] >= 24) {
            BossExplosion_Draw(
                patterns, boss->angle, (stones->position[STONE_NORTH].x + 8),
                (stones->position[STONE_NORTH].y + 8),
                (stones->kill_frame[STONE_NORTH] - 24));
        }
        if (stones->kill_frame[STONE_NORTH] < 32) {
            M_RollPut(
                patterns, stones->position[STONE_NORTH].x, vram_top,
                stones->sprite[STONE_NORTH]);
            stones->kill_frame[STONE_NORTH]++;
            return 0;
        }
        if (stones->kill_frame[STONE_NORTH] == 56) {
            boss->angle = -0x20;
        }
        patnum += ((stones->kill_frame[STONE_NORTH] - 32) / 8);
    } else {
        patnum += (stones->kill_frame[stone] / 8);
    }
    stones->kill_frame[stone]++;
    if (stones->kill_frame[stone] >= frames) {
        stones->kill_frame[stone] = 0;
        stones->flag[stone] = STONE_REMOVE;
        return 1;
    }
    M_Put(patterns, stones->position[stone].x, vram_top, patnum);
    return 0;
}

static void M_Hits(void)
{
    STONES *const stones = &m_Stones;
    int i;
    int damage;

    stones->pattern_frame = Math16_Wrap((int32_t)stones->pattern_frame + 1);
    for (i = 0; i < STONE_COUNT; i++) {
        if (stones->flag[i] > STONE_ACTIVE) {
            continue;
        }
        if ((damage =
                 M_Hit(stones->position[i].x, stones->position[i].y, 32, 40))
                == 0
            || (stones->flag[i] != STONE_ACTIVE)) {
            continue;
        }
        stones->flash[i] = 1;
        stones->damage[i] = Math16_Wrap((int32_t)stones->damage[i] + damage);
        if (((i == STONE_NORTH)            ? 930
                 : (i <= STONE_INNER_EAST) ? 140
                                           : 120)
            <= stones->damage[i]) {
            stones->flag[i] = STONE_KILL;
            Score_AddPoints(30000);
            if (i <= STONE_OUTER_EAST) {
                Items_Add(
                    (stones->position[i].x + 8), (stones->position[i].y + 12),
                    ((i == 0)       ? ITEM_BOMB
                         : (i <= 2) ? ITEM_BIG_POWER
                                    : ITEM_BOMB));
                SoundEffects_Play(SOUND_EFFECT_BOSS_HEAVY_HIT);
            } else {
                Player_BeginInvincibility(BOSS_DEFEAT_INVINCIBILITY_FRAMES);
            }
        }
    }
    for (i = 0; i < STONE_COUNT; i++) {
        if (stones->flag[i] == STONE_ACTIVE) {
            Shots_SetTarget((ENTITY_POINT) { stones->position[i].x + 8,
                                             stones->position[i].y + 8 });
            break;
        }
    }
    if (g_Player->current.y < (16 + 80)) {
        Bullets_AddPellets(
            g_Stage->scroll.back, stones->bullet.x, stones->bullet.y,
            (Random_RingU8And(0x0F) - 7), BULLET_AIMED, ((4 << 4) + 13));
    }
}

static void M_Rank(const uint8_t rank)
{
    STONES *const stones = &m_Stones;
    if (rank != 0) {
        stones->spread = BULLET_FAN5_WIDE_AIMED;
        stones->sprite_start = 0;
        stones->ring = BULLET_RING8;
        stones->firing_ring = BULLET_RING4;
    } else {
        stones->spread = BULLET_FAN3_WIDE_AIMED;
        stones->sprite_start = 2;
        stones->ring = BULLET_RING4;
        stones->firing_ring = BULLET_RING2;
    }
}

static void M_InnerAttacks(BOSS_STATE *const boss)
{
    STONES *const stones = &m_Stones;
    int i;

    if (boss->phase_frame < 30) {
        return;
    }
    if (boss->phase_frame < 180) {
        if ((boss->phase_frame % 24) == 0) {
            for (i = 0; i < 2; i++) {
                if (stones->flag[i] == STONE_ACTIVE) {
                    Bullets_AddPellets(
                        g_Stage->scroll.back, (stones->position[i].x + 12),
                        (stones->position[i].y + 8), 0x00, stones->spread,
                        ((2 << 4) + 6));
                }
            }
        }
    } else if (boss->phase_frame >= 200) {
        if (boss->phase_frame < 310) {
            if ((boss->phase_frame & 0xF) == 0) {
                for (i = 0; i < STONE_OUTER_WEST; i++) {
                    if (stones->flag[i] == STONE_ACTIVE) {
                        Bullets_AddSprites(
                            g_Stage->scroll.back, (stones->position[i].x + 12),
                            (stones->position[i].y + 8), 0x00, BULLET_AIMED, 81,
                            (5 << 4));
                    }
                }
            }
        } else if (boss->phase_frame > 350) {
            boss->phase_frame = 0;
        }
    }
}

static void M_OuterAttacks(BOSS_STATE *const boss, int stones_removed)
{
    if (boss->phase_frame < 30) {
        return;
    }
    if (boss->phase_frame < 200) {
        if ((boss->phase_frame % (110 - (stones_removed * 20))) == 0) {
            M_OuterSpread();
        }
    } else {
        if ((boss->phase_frame % (90 - (stones_removed * 20))) == 0) {
            M_OuterSpread();
        }
    }
}

static void M_PillarsRows(BOSS_STATE *const boss, const uint8_t rank)
{
    if (boss->phase_frame < 50) {
        return;
    }
    if (boss->phase_frame == 50) {
        Pillars_SetSpawnInterval(36);
        Pillars_Add(48, 96, 16, 0x67);
        Pillars_Add(192, 96, 16, 0x67);
        Pillars_Add(240, 96, 16, 0x67);
        Pillars_Add(384, 96, 16, 0x67);
    } else if (boss->phase_frame == 80) {
        Pillars_Add(80, 96, 16, 0x67);
        Pillars_Add(160, 96, 16, 0x67);
        Pillars_Add(272, 96, 16, 0x67);
        Pillars_Add(352, 96, 16, 0x67);
        boss->phase_frame = 0;
    }
}

static void M_Sweep(BOSS_STATE *const boss, const uint8_t rank)
{
    STONES *const stones = &m_Stones;
    if ((boss->phase_frame & 3) == 0) {
        Bullets_AddPellets(
            g_Stage->scroll.back, stones->bullet.x, stones->bullet.y,
            stones->sweep_angle, BULLET_MIRROR, ((4 << 4) + 6));
        stones->sweep_angle += 8;
        if (stones->sweep_angle > 0x82) {
            boss->phase_frame = 0;
            stones->sweep_angle = 0;
        }
    }
}

static void M_PillarsAimed(BOSS_STATE *const boss, const uint8_t rank)
{
    int16_t left;

    if (boss->phase_frame < 24) {
        return;
    }
    if ((boss->phase_frame % (28 - (rank * 3))) == 0) {
        Pillars_SetSpawnInterval(30);
        left = ((g_Player->current.x + 16) & 0xFFF0);
        Pillars_Add(left, 96, 16, 0x67);
    }
    if (boss->phase_frame >= 140) {
        boss->phase_frame = 0;
    }
}

static void M_AimedPairs(BOSS_STATE *const boss, const uint8_t rank)
{
    STONES *const stones = &m_Stones;
    uint8_t angle_1;
    uint8_t angle_2;

    if (boss->phase_frame < 40) {
        if ((boss->phase_frame % 6) == 0) {
            angle_1 = Math16_Angle(
                (g_Player->current.y - 32),
                (g_Player->current.x - stones->position[STONE_NORTH].x));
            angle_2 = (Random_RingU8And(0x1F) + 0x10);
            Bullets_AddPellets(
                g_Stage->scroll.back, stones->bullet.x, stones->bullet.y,
                (angle_1 + angle_2), BULLET_SINGLE, (5 << 4));
            Bullets_AddPellets(
                g_Stage->scroll.back, stones->bullet.x, stones->bullet.y,
                (angle_1 - angle_2), BULLET_SINGLE, (5 << 4));
        }
    } else if (boss->phase_frame < 102) {
        if ((boss->phase_frame % 6) == 0) {
            angle_1 = Math16_Angle(
                (g_Player->current.y - 32),
                (g_Player->current.x - stones->position[STONE_NORTH].x));
            angle_2 = (0x5F - boss->phase_frame);
            Bullets_AddPellets(
                g_Stage->scroll.back, stones->bullet.x, stones->bullet.y,
                (angle_1 + angle_2), BULLET_SINGLE, (5 << 4));
            Bullets_AddPellets(
                g_Stage->scroll.back, stones->bullet.x, stones->bullet.y,
                (angle_1 - angle_2), BULLET_SINGLE, (5 << 4));
        }
    } else {
        boss->phase_frame = 0;
    }
}

static void M_PillarsClosing(BOSS_STATE *const boss, const uint8_t rank)
{
    STONES *const stones = &m_Stones;
    if (boss->phase_frame < 16) {
        return;
    }
    if (boss->phase_frame == 16) {
        stones->pillar_left = 32;
        stones->pillar_state = 0;
    }
    if ((stones->pillar_state == 0) && ((boss->phase_frame & 7) == 0)) {
        Pillars_SetSpawnInterval(16);
        Pillars_Add(stones->pillar_left, 96, 3, 0x6F);
        Pillars_Add((432 - stones->pillar_left), 96, 3, 0x6F);
        stones->pillar_left += 16;
        if (stones->pillar_left > 176) {
            stones->pillar_state = 1;
            stones->pillar_left += 16;
        }
    }
    if (stones->pillar_state > 36) {
        if ((boss->phase_frame & 7) == 0) {
            Pillars_Add(stones->pillar_left, 96, 1, 0x6F);
            Pillars_Add((432 - stones->pillar_left), 96, 1, 0x6F);
            stones->pillar_left -= 16;
            if (stones->pillar_left < 64) {
                stones->pillar_state = 0;
                boss->phase_frame = 0;
                stones->pillar_left = 32;
            }
        }
    } else if (stones->pillar_state != 0) {
        stones->pillar_state++;
    }
}

static void M_Gravity(BOSS_STATE *const boss, const uint8_t rank)
{
    STONES *const stones = &m_Stones;
    int i;

    if ((boss->phase_frame & 0xF) == 0) {
        for (i = stones->sprite_start; i < STONE_NORTH; i++) {
            Bullets_AddSprites(
                g_Stage->scroll.back, stones->position[i].x,
                stones->position[i].y, Random_RingU8(), BULLET_MOTION_GRAVITY,
                82, (2 << 4));
        }
    }
    if (boss->phase_frame > 110) {
        boss->phase_frame = 0;
    }
}

static void M_RandomRings(BOSS_STATE *const boss, const uint8_t rank)
{
    STONES *const stones = &m_Stones;
    if ((boss->phase_frame & 3) == 0) {
        Bullets_AddPellets(
            g_Stage->scroll.back, stones->bullet.x, stones->bullet.y,
            Random_RingU8(), stones->firing_ring,
            ((boss->phase_frame >> 1) + ((1 << 4) + 14)));
    }
    if (boss->phase_frame > 130) {
        boss->phase_frame = 0;
    }
}

static void M_PillarsRandom(BOSS_STATE *const boss, const uint8_t rank)
{
    STONES *const stones = &m_Stones;
    int i;
    int column;

    if (boss->phase_frame < 16) {
        return;
    }
    if (boss->phase_frame < 80) {
        if ((boss->phase_frame % 36) == 0) {
            Pillars_SetSpawnInterval(30);
            column = ((Random_RingU8() % 24) + 2);
            Pillars_Add((column * 16), 96, 12, 0x6F);
        }
        if (((boss->phase_frame % 40) == 0) && (rank != 0)) {
            Bullets_AddPellets(
                g_Stage->scroll.back, stones->bullet.x, stones->bullet.y, 0x00,
                BULLET_AIMED, ((4 << 4) + 6));
        }
    } else if (boss->phase_frame < 120) {
        if (boss->phase_frame == 100) {
            Pillars_SetSpawnInterval(30);
            for (i = 0; i < 3; i++) {
                column = ((Random_RingU8() % 24) + 2);
                Pillars_Add((column * 16), 96, 12, 0x6F);
            }
        }
    } else {
        boss->phase_frame = 0;
    }
}

static void M_SpritePairs(BOSS_STATE *const boss, const uint8_t rank)
{
    STONES *const stones = &m_Stones;
    int i;

    if (boss->phase_frame < 16) {
        return;
    }
    if (boss->phase_frame == 16) {
        stones->fan_angle = 0x29;
    }
    if ((boss->phase_frame % 30) == 0) {
        for (i = 0; i < STONE_NORTH; i++) {
            Bullets_AddSprites(
                g_Stage->scroll.back, stones->position[i].x,
                stones->position[i].y, stones->fan_angle, BULLET_AIMED, 82,
                ((3 << 4) + 12));
            Bullets_AddSprites(
                g_Stage->scroll.back, stones->position[i].x,
                stones->position[i].y, -stones->fan_angle, BULLET_AIMED, 82,
                ((3 << 4) + 12));
        }
        stones->fan_angle -= 3;
    }
    if (stones->fan_angle >= 0xC8) {
        boss->phase_frame = 0;
    }
}

static void M_RingPairs(BOSS_STATE *const boss, const uint8_t rank)
{
    STONES *const stones = &m_Stones;
    if (boss->phase_frame == 2) {
        stones->ring_angle = 0x3E;
    }
    if ((boss->phase_frame & 7) == 0) {
        Bullets_AddPellets(
            g_Stage->scroll.back, stones->bullet.x, stones->bullet.y,
            (0x00 - stones->ring_angle), stones->ring, (4 << 4));
        Bullets_AddPellets(
            g_Stage->scroll.back, stones->bullet.x, stones->bullet.y,
            stones->ring_angle, stones->ring, (4 << 4));
        stones->ring_angle -= 3;
        if (stones->ring_angle > 0xC8) {
            boss->phase_frame = 0;
        }
    }
}

static void M_AimedStreams(BOSS_STATE *const boss, const uint8_t rank)
{
    STONES *const stones = &m_Stones;
    int i;

    if (boss->phase_frame < 60) {
        return;
    }
    if ((boss->phase_frame % 64) == 0) {
        for (i = 0; i < STONE_COUNT; i++) {
            stones->aim_angle[i] = Math16_Angle(
                (g_Player->current.y - 32),
                (g_Player->current.x - stones->position[i].x));
        }
    }
    if ((boss->phase_frame & 3) == 0) {
        if (boss->phase_frame < 384) {
            if (((boss->phase_frame % 64) >> 2) <= 8) {
                for (i = 0; i < STONE_COUNT; i++) {
                    Bullets_AddPellets(
                        g_Stage->scroll.back, (stones->position[i].x + 12),
                        (stones->position[i].y + 12), stones->aim_angle[i],
                        BULLET_SINGLE, (7 << 4));
                }
                SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
            }
        } else {
            Bullets_SetSpawnLayers(0);
            for (i = 0; i < STONE_COUNT; i++) {
                Bullets_AddSprites(
                    g_Stage->scroll.back, (stones->position[i].x + 8),
                    (stones->position[i].y + 8), stones->aim_angle[STONE_NORTH],
                    BULLET_RING4, 86, (7 << 4));
            }
            SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        }
    }
    if (boss->phase_frame >= 447) {
        boss->phase_frame = 0;
        Bullets_SetSpawnLayers(1);
    }
}

static int M_Open(int stone)
{
    STONES *const stones = &m_Stones;
    if ((stone == STONE_INNER_WEST) || (stone == STONE_INNER_EAST)) {
        stones->sprite[stone]++;
        if (stones->sprite[stone] >= 155) {
            stones->flag[stone] = STONE_ACTIVE;
            return 1;
        }
    } else if ((stone == STONE_OUTER_WEST) || (stone == STONE_OUTER_EAST)) {
        if (stones->sprite[stone] < 152) {
            stones->sprite[stone]++;
        }
        if (stones->sprite[stone] == 152) {
            stones->sprite[stone] = 156;
        } else {
            stones->sprite[stone]++;
        }
        if (stones->sprite[stone] >= 159) {
            stones->flag[stone] = STONE_ACTIVE;
            return 1;
        }
    } else if (stone == STONE_NORTH) {
        if (stones->sprite[STONE_NORTH] < 152) {
            stones->sprite[STONE_NORTH]++;
        }
        if (stones->sprite[STONE_NORTH] == 152) {
            stones->sprite[STONE_NORTH] = 160;
        } else {
            stones->sprite[STONE_NORTH]++;
        }
        if (stones->sprite[STONE_NORTH] >= 163) {
            stones->flag[STONE_NORTH] = STONE_ACTIVE;
            return 1;
        }
    }
    return 0;
}

static int M_Close(void)
{
    STONES *const stones = &m_Stones;
    if (stones->sprite[STONE_NORTH] >= 160) {
        stones->sprite[STONE_NORTH]--;
    }
    if (stones->sprite[STONE_NORTH] == 159) {
        stones->flag[STONE_NORTH] = STONE_DORMANT;
        stones->sprite[STONE_NORTH] = 151;
    } else {
        stones->sprite[STONE_NORTH]--;
    }
    if (stones->sprite[STONE_NORTH] <= 148) {
        return 1;
    }
    return 0;
}

static bool M_Ring(
    BOSS_STATE *const boss, const int16_t first, const int16_t second,
    const int16_t third)
{
    STONES *const stones = &m_Stones;
    if (boss->phase_frame < 49) {
        return false;
    }
    if (boss->phase_frame == 49) {
        memset(stones->ring_flag, 0, sizeof(stones->ring_flag));
        stones->ring_index = 0;
        stones->ring_phase = 0;
        stones->ring_done = 0;
        stones->ring_tile[0] = first;
        stones->ring_tile[1] = second;
        stones->ring_tile[2] = third;
    } else if (stones->ring_phase <= 2 && (boss->phase_frame & 1) != 0) {
        ASSERT(stones->ring_index < TILE_COLUMNS / 2);
        stones->ring_flag[stones->ring_index] = stones->ring_phase + 1;
        stones->ring_flag[TILE_COLUMNS - 1 - stones->ring_index] =
            stones->ring_phase + 1;
        stones->ring_index++;
        if (stones->ring_index >= TILE_COLUMNS / 2) {
            stones->ring_phase++;
            stones->ring_index = 0;
        }
    }
    for (int i = 0; i < TILE_COLUMNS; i++) {
        const uint8_t flag = stones->ring_flag[i];
        if (flag >= 1 && flag <= 3) {
            Background_SetTile(
                g_Stage->scroll.front, g_Stage->scroll.back, 32 + i * 16,
                stones->ring_y, (uint8_t)stones->ring_tile[flag - 1]);
            stones->ring_flag[i] = 0;
            if (flag == 3) {
                stones->ring_done++;
            }
        }
    }
    Grcg_Off();
    return stones->ring_done >= TILE_COLUMNS;
}

static bool M_RingOpen(BOSS_STATE *const boss)
{
    return M_Ring(boss, 41, 42, 43);
}

static bool M_RingClose(BOSS_STATE *const boss)
{
    return M_Ring(boss, 42, 41, 40);
}

static void Stones_Init(void)
{
    memset(&m_Stones, 0, sizeof(m_Stones));
}

static bool Stones_MidbossInvalidate(void)
{
    unsigned removed = 0;
    for (int i = 0; i < STONE_MIDBOSS_COUNT; i++) {
        if (m_Stones.flag[i] == STONE_MIDBOSS_REMOVED) {
            removed++;
            continue;
        }
        const ENTITY_POINT position = *M_MidbossPosition(i);
        Background_Mark(g_Stage->scroll.line, position.x, position.y, 64, 32);
        *M_MidbossPosition(i) =
            m_Stones.midboss_position[i][g_Stage->scroll.front];
    }
    return removed != STONE_MIDBOSS_COUNT;
}

static void Stones_MidbossUpdate(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns,
    const uint8_t rank)
{
    STONES *const stones = &m_Stones;
    const int target = stones->flag[0] == STONE_MIDBOSS_ACTIVE ? 0 : 1;
    const ENTITY_POINT position = *M_MidbossPosition(target);
    Shots_SetTarget((ENTITY_POINT) { Math16_Wrap((int32_t)position.x + 24),
                                     Math16_Wrap((int32_t)position.y + 16) });
    boss->phase_frame = Math16_Wrap((int32_t)boss->phase_frame + 1);
    const int16_t frame = boss->phase_frame;
    if (frame == 1) {
        for (int i = 0; i < STONE_MIDBOSS_COUNT; i++) {
            for (unsigned page = 0; page < 2; page++) {
                stones->midboss_position[i][page] =
                    (ENTITY_POINT) { i == 0 ? 32 : 352, -16 };
            }
            stones->damage[i] = 0;
        }
        stones->midboss_rounds = 0;
    } else if (frame < 128) {
        stones->sprite[0] = (g_Stage->scroll.line & 3) + 144;
        for (int i = 0; i < STONE_MIDBOSS_COUNT; i++) {
            ENTITY_POINT *const p = M_MidbossPosition(i);
            p->y = Math16_Wrap((int32_t)p->y + g_Stage->scroll.moved);
            const bool hit = M_MidbossHit(i) != 0;
            M_MidbossDraw(patterns, i, hit);
        }
    } else {
        stones->sprite[0] = (g_Stage->scroll.line & 3) + 144;
        if (frame == 128) {
            stones->midboss_rounds++;
        }
        for (int i = 0; i < STONE_MIDBOSS_COUNT; i++) {
            if (stones->flag[i] == STONE_MIDBOSS_KILL) {
                M_MidbossDefeat(patterns, i);
            } else if (stones->flag[i] == STONE_MIDBOSS_ACTIVE) {
                M_MidbossFire(boss, rank, i);
                if (M_MidbossHit(i) != 0) {
                    if (stones->damage[i] <= 280) {
                        M_MidbossDraw(patterns, i, true);
                    } else {
                        SoundEffects_Play(SOUND_EFFECT_BOSS_HEAVY_HIT);
                        stones->flag[i] = STONE_MIDBOSS_KILL;
                        Score_AddPoints(20000);
                    }
                } else {
                    M_MidbossDraw(patterns, i, false);
                }
                if (stones->midboss_rounds >= 4) {
                    stones->flag[i] = STONE_MIDBOSS_KILL;
                }
            }
        }
    }
}

static void Stones_Begin(BOSS_STATE *const boss, const uint8_t rank)
{
    STONES *const stones = &m_Stones;
    boss->phase_frame = 0;
    for (int i = 0; i < STONE_COUNT; i++) {
        stones->flag[i] = STONE_DORMANT;
        Boss_ResetDamage(
            &stones->damage[i],
            i == STONE_NORTH            ? 930
                : i <= STONE_INNER_EAST ? 140
                                        : 120);
        stones->sprite[i] = 148;
        stones->flash[i] = 0;
    }
    stones->position[STONE_INNER_WEST] = (ENTITY_POINT) { 128, 32 };
    stones->position[STONE_INNER_EAST] = (ENTITY_POINT) { 288, 32 };
    stones->position[STONE_OUTER_WEST] = (ENTITY_POINT) { 48, 48 };
    stones->position[STONE_OUTER_EAST] = (ENTITY_POINT) { 368, 48 };
    stones->position[STONE_NORTH] = (ENTITY_POINT) { 208, 32 };
    stones->bullet = (ENTITY_POINT) { 220, 40 };
    Enemies_RemoveAll();
    Pillars_SetSpawnInterval(12);
    stones->pattern_frame = 0;
    stones->frames = 0;
    stones->phase = STONES_OPEN_INNER;
    stones->pattern = 0;
    boss->angle = 32;
    stones->ring_y = M_Top(96);
    M_Rank(rank);
    Pillars_Enable();
}

static void Stones_Invalidate(void)
{
    STONES *const stones = &m_Stones;
    int i;

    for (i = 0; i < STONE_COUNT; i++) {
        if (stones->flag[i] < STONE_REMOVED) {
            Background_Mark(
                g_Stage->scroll.line, stones->position[i].x,
                stones->position[i].y, 32, 32);
            if (stones->flag[i] == STONE_REMOVE) {
                stones->flag[i] = STONE_REMOVED;
            }
        }
    }
}

static BOSS_RESULT Stones_Update(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns,
    const uint8_t rank)
{
    STONES *const stones = &m_Stones;
    int i;
    int removed = 0;

    boss->phase_frame = Math16_Wrap((int32_t)boss->phase_frame + 1);
    stones->frames = (int32_t)((uint32_t)stones->frames + 1);
    M_Hits();
    M_Draw(patterns);
    for (i = 0; i < STONE_COUNT; i++) {
        if (stones->flag[i] == STONE_KILL) {
            M_Defeat(boss, patterns, i);
        } else if (stones->flag[i] == STONE_REMOVED) {
            removed++;
        }
    }
    if (removed >= STONE_COUNT) {
        return BOSS_FINISHED;
    }
    if (stones->phase == STONES_OPEN_INNER) {
        if ((boss->phase_frame == 50) || (boss->phase_frame == 52)
            || (boss->phase_frame == 54) || (boss->phase_frame == 58)
            || (boss->phase_frame == 60) || (boss->phase_frame == 62)) {
            stones->sprite[STONE_INNER_WEST]++;
            stones->sprite[STONE_INNER_EAST]++;
        } else if (boss->phase_frame == 64) {
            stones->sprite[STONE_INNER_WEST]++;
            stones->sprite[STONE_INNER_EAST]++;
            boss->phase_frame = 0;
            stones->flag[STONE_INNER_WEST] = STONE_ACTIVE;
            stones->flag[STONE_INNER_EAST] = STONE_ACTIVE;
            stones->phase = STONES_INNER;
            stones->pattern_frame = 0;
        }
    } else if (stones->phase == STONES_INNER) {
        M_InnerAttacks(boss);
        if (stones->frames > 1300) {
            if (stones->flag[STONE_INNER_WEST] == STONE_ACTIVE) {
                stones->flag[STONE_INNER_WEST] = STONE_KILL;
            }
            if (stones->flag[STONE_INNER_EAST] == STONE_ACTIVE) {
                stones->flag[STONE_INNER_EAST] = STONE_KILL;
            }
        }
        if ((removed >= 2) && ((boss->phase_frame % 3) == 0)) {
            M_Open(STONE_OUTER_WEST);
            if (M_Open(STONE_OUTER_EAST)) {
                stones->phase = STONES_OUTER;
                boss->phase_frame = 0;
                stones->pattern_frame = 0;
            }
            stones->frames = 0;
        }
    } else if (stones->phase == STONES_OUTER) {
        if (stones->frames > 1300) {
            if (stones->flag[STONE_OUTER_WEST] == STONE_ACTIVE) {
                stones->flag[STONE_OUTER_WEST] = STONE_KILL;
            }
            if (stones->flag[STONE_OUTER_EAST] == STONE_ACTIVE) {
                stones->flag[STONE_OUTER_EAST] = STONE_KILL;
            }
        }
        if (removed >= 4) {
            stones->phase = STONES_OPEN_NORTH;
            boss->phase_frame = 0;
            stones->pattern = 0;
            stones->pattern_frame = 0;
            Shots_SetTarget(
                (ENTITY_POINT) { stones->position[STONE_NORTH].x + 8, 32 });
        } else {
            M_OuterAttacks(boss, removed);
        }
    } else if (stones->phase == STONES_OPEN_NORTH) {
        if (M_RingOpen(boss) && ((boss->phase_frame % 3) == 0)
            && M_Open(STONE_NORTH)) {
            stones->phase = STONES_NORTH_FIRST;
            boss->phase_frame = 0;
            stones->pattern_frame = 0;
            stones->frames = 0;
        }
    } else if (stones->phase == STONES_NORTH_FIRST) {
        static const STONE_ATTACK first[] = {
            M_PillarsRows, M_Sweep,       M_PillarsAimed,
            M_Gravity,     M_RandomRings, M_PillarsRandom,
        };
        if (stones->pattern < ARRAY_SIZE(first)) {
            first[stones->pattern](boss, rank);
        }
        if (boss->phase_frame == 0) {
            stones->pattern++;
            stones->pattern_frame = 0;
            if (stones->pattern >= ARRAY_SIZE(first)) {
                stones->phase = STONES_CLOSE_NORTH;
                stones->pattern = 0;
            }
        }
        if (stones->damage[STONE_NORTH] >= 500) {
            stones->damage[STONE_NORTH] = 500;
        }
    } else if (stones->phase == STONES_CLOSE_NORTH) {
        if (((boss->phase_frame % 3) == 0) && (stones->pattern == 0)) {
            stones->pattern = M_Close();
        }
        if (M_RingClose(boss)) {
            stones->phase = STONES_NORTH_CLOSED;
            boss->phase_frame = 0;
            stones->pattern_frame = 0;
            stones->frames = 0;
            stones->pattern = 0;
        }
    } else if (stones->phase == STONES_NORTH_CLOSED) {
        static const STONE_ATTACK closed[] = {
            M_RingPairs,
            M_SpritePairs,
            M_RingPairs,
            M_AimedStreams,
        };
        if (stones->pattern < ARRAY_SIZE(closed)) {
            closed[stones->pattern](boss, rank);
        }
        if (boss->phase_frame == 0) {
            stones->pattern++;
            stones->pattern_frame = 0;
            if (stones->pattern >= ARRAY_SIZE(closed)) {
                stones->phase = STONES_REOPEN_NORTH;
            }
        }
    } else if (stones->phase == STONES_REOPEN_NORTH) {
        if (M_RingOpen(boss) && ((boss->phase_frame % 3) == 0)
            && M_Open(STONE_NORTH)) {
            stones->phase = STONES_NORTH_FINAL;
            boss->phase_frame = 0;
            stones->pattern_frame = 0;
            stones->frames = 0;
            stones->pattern = 0;
        }
    } else if (stones->phase == STONES_NORTH_FINAL) {
        static const STONE_ATTACK final[] = {
            M_PillarsClosing, M_Sweep,       M_AimedPairs,
            M_Gravity,        M_RandomRings, M_PillarsRandom,
        };
        if (stones->pattern < ARRAY_SIZE(final)) {
            final[stones->pattern](boss, rank);
        }
        if (boss->phase_frame == 0) {
            stones->pattern++;
            stones->pattern_frame = 0;
            if (stones->pattern >= ARRAY_SIZE(final)) {
                stones->pattern = 1;
            }
            if (stones->frames > 2000) {
                stones->flag[STONE_NORTH] = STONE_KILL;
            }
        }
    }
    return BOSS_RUNNING;
}

static void M_Begin(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const TH02_SESSION *const session)
{
    Stones_Begin(p_BossState, (uint8_t)session->rank);
}

static void M_Invalidate(const bool reduce_effects)
{
    Stones_Invalidate();
}

static BOSS_RESULT M_Update(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, TH02_SESSION *const session,
    const TH02_INPUT *const input)
{
    return Stones_Update(p_BossState, patterns, (uint8_t)session->rank);
}

static bool M_MidbossInvalidate(void)
{
    return Stones_MidbossInvalidate();
}

static void M_MidbossUpdate(
    const PATTERN_BANK *const patterns, const uint8_t rank)
{
    Stones_MidbossUpdate(p_BossState, patterns, rank);
}

const BOSS_STRATEGY g_BossStones = {
    .init = Stones_Init,
    .begin = M_Begin,
    .invalidate = M_Invalidate,
    .update = M_Update,
    .post_dialogue = true,
    .midboss_invalidate = M_MidbossInvalidate,
    .midboss_update = M_MidbossUpdate,
    .talk = { .music_after = true },
};
