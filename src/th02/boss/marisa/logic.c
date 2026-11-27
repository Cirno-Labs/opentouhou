#include "th02/background.h"
#include "th02/boss/explosion.h"
#include "th02/boss/marisa.h"
#include "th02/boss/marisa/assets.h"
#include "th02/boss/private.h"
#include "th02/boss/scene.h"
#include "th02/boss/strategy.h"
#include "th02/boss/talk.h"
#include "th02/bullets.h"
#include "th02/clip.h"
#include "th02/dialogue/render.h"
#include "th02/dialogue/sequence.h"
#include "th02/dialogue/summon.h"
#include "th02/gameplay/memory.h"
#include "th02/items.h"
#include "th02/palette.h"
#include "th02/pillars.h"
#include "th02/player.h"
#include "th02/random.h"
#include "th02/score.h"
#include "th02/shots.h"
#include "th02/sound_effects.h"
#include "th02/sparks.h"
#include "th02/stage.h"
#include "th02/video.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

#define MARISA_BIT_COUNT 4
#define MARISA_DOT_COUNT 30

typedef struct {
    uint8_t flag;
    uint8_t angle;
    ENTITY_POINT position[2];
    uint8_t size;
    uint8_t speed;
} MARISA_DOT;

typedef enum {
    MARISA_MIDBOSS_ENTER,
    MARISA_MIDBOSS_TRAVERSE,
} MARISA_MIDBOSS_PHASE;

typedef enum {
    MARISA_MIDBOSS_PELLETS,
    MARISA_MIDBOSS_FAN,
    MARISA_MIDBOSS_CHASE,
    MARISA_MIDBOSS_PILLARS,
    MARISA_MIDBOSS_RANDOM,
    MARISA_MIDBOSS_ATTACK_COUNT,
} MARISA_MIDBOSS_ATTACK;

typedef enum {
    MARISA_OPENING,
    MARISA_OPEN_ORBS,
    MARISA_COMBAT,
} MARISA_PHASE;

typedef struct {
    MARISA_DOT dots[MARISA_DOT_COUNT];
    ENTITY_POINT position;
    int16_t phase;
    int16_t direction;
    int16_t midboss_bullet_top;
    int16_t midboss_kill_frame;
    uint8_t midboss_pattern;
    uint8_t midboss_patterns_done;
    uint8_t midboss_page;
    bool midboss_active;
    ENTITY_POINT bit_position[2][MARISA_BIT_COUNT];
    ENTITY_POINT orbit_center;
    int16_t bit_hits[MARISA_BIT_COUNT];
    int16_t bit_state[MARISA_BIT_COUNT];
    int16_t bit_hit[MARISA_BIT_COUNT];
    int16_t bit_flash[MARISA_BIT_COUNT];
    int16_t bit_radius[MARISA_BIT_COUNT];
    int16_t bit_speed[MARISA_BIT_COUNT];
    int16_t bit_state_sum;
    int16_t defeat_frame;
    int16_t move_x;
    int16_t move_y;
    uint8_t bit_angle[MARISA_BIT_COUNT];
    uint8_t bit_spin[MARISA_BIT_COUNT];
    uint8_t damage_enable;
    uint8_t drift_toggle;
    uint8_t double_fire;
    uint8_t double_angle;
    uint8_t drift_angle;
    uint8_t orbit_angle, expand_angle, spin_angle, ring_toggle,
        background_color;
    uint8_t fan, second_fan, unused_fan, edge_count, pattern_rounds,
        bit_patterns_seen, bomb_mask;
    int8_t edge_direction;
    int8_t orbit_direction;
    int8_t color_index;
    int8_t pattern;
    int8_t pattern_cycle;
} MARISA;

typedef enum {
    MARISA_PATTERN_DRIFT = -3,
    MARISA_PATTERN_AIMED_FANS,
    MARISA_PATTERN_PELLETS,
    MARISA_PATTERN_OPEN_ORBS,
    MARISA_PATTERN_ORBIT,
    MARISA_PATTERN_EXPAND,
    MARISA_PATTERN_SPIN,
    MARISA_PATTERN_EDGE,
    MARISA_PATTERN_SPLIT,
    MARISA_PATTERN_DETONATE,
    MARISA_PATTERN_CHOOSE = 127,
} MARISA_PATTERN;

typedef struct {
    void (*update)(BOSS_STATE *boss, uint8_t rank);
    bool move;
} MARISA_ATTACK;

static MARISA m_Marisa;
static const uint8_t m_BackgroundColors[] = { 10, 12, 13, 13, 12, 10 };

static void M_DotsInit(void)
{
    for (int i = 0; i < MARISA_DOT_COUNT; i++) {
        m_Marisa.dots[i].flag = ENTITY_FREE;
    }
}

static const PATTERN *M_Pattern(
    const PATTERN_BANK *const patterns, const int16_t id)
{
    ASSERT(id >= 0 && id < PATTERN_LIMIT);
    const PATTERN *const pattern = &patterns->entries[id];
    ASSERT(pattern->data != nullptr);
    return pattern;
}

static ENTITY_POINT *M_Position(BOSS_STATE *const boss)
{
    return &boss->position[m_Marisa.midboss_page];
}

static int16_t M_Top(void)
{
    int16_t top =
        Math16_Wrap((int32_t)m_Marisa.position.y + g_Stage->scroll.line);
    if (top >= 400) {
        top -= 400;
    }
    return top;
}

static int16_t M_BulletLeft(void)
{
    return Math16_Wrap(
        (int32_t)m_Marisa.direction * 16 + m_Marisa.position.x + 28);
}

static void M_Pellets(
    const int16_t top, const uint8_t angle, const uint8_t pattern,
    const int16_t speed)
{
    Bullets_AddPellets(
        g_Stage->scroll.back, M_BulletLeft(), top, angle, pattern, speed);
}

static void M_MarisaRender(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    int16_t top =
        Math16_Wrap((int32_t)m_Marisa.position.y + g_Stage->scroll.line);
    if (top < 0) {
        top = Math16_Wrap((int32_t)top + 400);
    } else if (top >= 400) {
        top -= 400;
    }
    if (boss->hit_flash != 0) {
        SoundEffects_Play(SOUND_EFFECT_HIT);
        Draw_SpriteRollPlane(
            M_Pattern(patterns, boss->sprite), m_Marisa.position.x, top, 0,
            0xffc0);
        boss->hit_flash = 0;
    } else {
        Draw_SpriteRoll(
            M_Pattern(patterns, boss->sprite), m_Marisa.position.x, top);
    }
}

static void M_MidbossDefeat(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    MARISA *const marisa = &m_Marisa;
    if (boss->phase_frame % 24 == 0) {
        Sparks_Add(
            g_Stage->scroll.back, Math16_Wrap((int32_t)marisa->position.x + 32),
            (uint16_t)(marisa->position.y + 32), 128, 24, true);
    }
    const int16_t image = 10 + (marisa->midboss_kill_frame >> 3);
    marisa->midboss_kill_frame =
        Math16_Wrap((int32_t)marisa->midboss_kill_frame + 1);
    if (marisa->midboss_kill_frame >= 64) {
        marisa->midboss_kill_frame = 0;
        boss->phase_frame = 0;
        boss->defeated = 2;
        Shots_ResetTarget();
        marisa->midboss_active = false;
        marisa->direction = -1;
        Stage_SetMidbossRow(1632);
    } else {
        Draw_SpriteZoom(
            M_Pattern(patterns, image), marisa->position.x, M_Top(), 2);
    }
}

static void M_MidbossHit(BOSS_STATE *const boss)
{
    MARISA *const marisa = &m_Marisa;
    const int16_t damage = Boss_Hit(
        &boss->damage, Math16_Wrap((int32_t)marisa->position.x + 8),
        marisa->position.y, 48, 48, 1, 1);
    if (damage != 0) {
        boss->hit_flash = 1;
        const int16_t top = M_Top();
        if (boss->damage >= 400 && top < 336 && top > 96) {
            SoundEffects_Play(SOUND_EFFECT_BOSS_HEAVY_HIT);
            boss->defeated = 1;
            Score_AddPoints(50000);
            Items_Add(
                Math16_Wrap((int32_t)marisa->position.x + 24),
                marisa->position.y,
                g_Stage->world.midboss_row == 1632 ? ITEM_LIFE : ITEM_BOMB);
        }
    }
}

static void M_MidbossPellets(const int16_t frame, const uint8_t rank)
{
    if (frame % 16 == 0) {
        SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        M_Pellets(96, 64, BULLET_SINGLE, 80);
    }
}

static void M_MidbossFan(const int16_t frame, const uint8_t rank)
{
    if (frame % 16 == 0) {
        SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        M_Pellets(
            96, 64, rank == 0 ? BULLET_FAN4_WIDE : BULLET_FAN5_MEDIUM,
            Math16_Wrap((int32_t)frame - 20));
    }
}

static void M_MidbossChase(const int16_t frame, const uint8_t rank)
{
    if (frame % 8 == 0) {
        Bullets_AddSprites(
            g_Stage->scroll.back, M_BulletLeft(), 96, 64, BULLET_MOTION_CHASE,
            80, 8);
    }
}

static void M_MidbossPillars(const int16_t frame, const uint8_t rank)
{
    if (frame % (16 - rank * 2) == 0) {
        Pillars_SetSpawnInterval(16);
        Pillars_Add(M_BulletLeft(), 80, 6, 111);
    }
}

static void M_MidbossRandom(const int16_t frame, const uint8_t rank)
{
    if (frame % 16 == 0) {
        SoundEffects_Play(SOUND_EFFECT_PILLAR_FALL);
        for (int i = 0; i < 4; i++) {
            const uint8_t angle = Random_RingU8And(63) + 32;
            M_Pellets(m_Marisa.midboss_bullet_top, angle, BULLET_SINGLE, 70);
        }
    }
}

static void M_MidbossFire(BOSS_STATE *const boss, const uint8_t rank)
{
    static void (*const attacks[MARISA_MIDBOSS_ATTACK_COUNT])(
        int16_t, uint8_t) = {
        [MARISA_MIDBOSS_PELLETS] = M_MidbossPellets,
        [MARISA_MIDBOSS_FAN] = M_MidbossFan,
        [MARISA_MIDBOSS_CHASE] = M_MidbossChase,
        [MARISA_MIDBOSS_PILLARS] = M_MidbossPillars,
        [MARISA_MIDBOSS_RANDOM] = M_MidbossRandom,
    };
    if (m_Marisa.midboss_pattern < ARRAY_SIZE(attacks)) {
        attacks[m_Marisa.midboss_pattern](boss->phase_frame, rank);
    }
}

static void M_Turn(BOSS_STATE *const boss)
{
    boss->phase_frame = 2;
    boss->sprite = 148;
    m_Marisa.direction = Math16_Wrap(-(int32_t)m_Marisa.direction);
    m_Marisa.midboss_pattern = Random_RingU8() % MARISA_MIDBOSS_ATTACK_COUNT;
}

static ENTITY_POINT *M_BossPosition(BOSS_STATE *const boss)
{
    return &boss->position[g_Stage->scroll.back];
}

static int16_t *M_BitX(const int bit)
{
    return &m_Marisa.bit_position[g_Stage->scroll.back][bit].x;
}

static int16_t *M_BitY(const int bit)
{
    return &m_Marisa.bit_position[g_Stage->scroll.back][bit].y;
}

static void M_Star(
    const int16_t left, const int16_t top, const uint8_t angle,
    const uint8_t pattern)
{
    Bullets_AddSprites(g_Stage->scroll.back, left, top, angle, pattern, 85, 70);
}

static void M_Rect(
    const PATTERN_BANK *const patterns, const int16_t left, const int16_t top,
    const int16_t sprite)
{
    Draw_SpriteRect(M_Pattern(patterns, sprite), left, top);
}

static void M_Zoom(
    const PATTERN_BANK *const patterns, const int16_t left, const int16_t top,
    const int16_t sprite, const int16_t scale)
{
    Draw_SpriteZoom(M_Pattern(patterns, sprite), left, top, scale);
}

static int16_t M_RawHit(
    const int16_t left, const int16_t top, const int16_t width,
    const int16_t height)
{
    int16_t ignored = 0;
    return Boss_Hit(&ignored, left, top, width, height, 0, m_Marisa.bomb_mask);
}

static void M_BitSpeed(const int16_t even, const int16_t odd)
{
    m_Marisa.bit_speed[0] = m_Marisa.bit_speed[2] = even;
    m_Marisa.bit_speed[1] = m_Marisa.bit_speed[3] = odd;
}

static void M_DotAdd(const int16_t left, const int16_t top, const uint8_t angle)
{
    for (int i = 0; i < MARISA_DOT_COUNT; i++) {
        MARISA_DOT *const dot = &m_Marisa.dots[i];
        if (dot->flag == ENTITY_FREE) {
            dot->flag = ENTITY_ACTIVE;
            dot->angle = angle;
            dot->position[0] = dot->position[1] =
                (ENTITY_POINT) { Math16_FromPixel(left),
                                 Math16_FromPixel(top) };
            dot->size = 1;
            dot->speed = 24;
            break;
        }
    }
}

static void M_DotsInvalidate(const bool reduce_effects)
{
    Grcg_SetColor(0);
    for (int i = 0; i < MARISA_DOT_COUNT; i++) {
        MARISA_DOT *const dot = &m_Marisa.dots[i];
        if (dot->flag == ENTITY_FREE) {
            continue;
        }
        if (!reduce_effects || g_Stage->scroll.back == (i & 1)) {
            const ENTITY_POINT old = dot->position[g_Stage->scroll.back];
            const int16_t left = Math16_ToPixel(old.x);
            const uint16_t scaled =
                (uint16_t)((uint16_t)Math16_ToPixel(old.y) << 6);
            uint16_t offset =
                (uint16_t)(left / 8 - (left % 8 < 0) + scaled + (scaled >> 2));
            uint8_t *const page = g_Video.pages[g_Video.drawing_page];
            for (unsigned row = 0; row < dot->size; row++) {
                for (unsigned byte = 0; byte < 2; byte++) {
                    const uint32_t address =
                        ((uint32_t)offset + byte) % VIDEO_PLANE_SIZE;
                    memset(page + address * 8, 0, 8);
                }
                offset = (uint16_t)(offset + 80);
            }
        }
        if (dot->flag == ENTITY_REMOVE) {
            dot->flag = ENTITY_FREE;
        } else {
            dot->position[g_Stage->scroll.front ^ 1] =
                dot->position[g_Stage->scroll.front];
        }
    }
    Grcg_Off();
}

static void M_DotsUpdate(const bool reduce_effects)
{
    Grcg_SetColor(m_Marisa.background_color);
    for (int i = 0; i < MARISA_DOT_COUNT; i++) {
        MARISA_DOT *const dot = &m_Marisa.dots[i];
        if (dot->flag != ENTITY_ACTIVE) {
            continue;
        }
        dot->speed += 4;
        const MATH16_VECTOR velocity = Math16_Vector(dot->speed, dot->angle);
        ENTITY_POINT *const position = &dot->position[g_Stage->scroll.back];
        position->x = Math16_Wrap((int32_t)position->x + velocity.x);
        position->y = Math16_Wrap((int32_t)position->y + velocity.y);
        const int16_t left = Math16_ToPixel(position->x);
        const int16_t top = Math16_ToPixel(position->y);
        if (left >= 416 || left <= 24 || top <= 8 || top >= 384) {
            dot->flag = ENTITY_REMOVE;
            continue;
        }
        int threshold = 120;
        dot->size = 5;
        while (threshold > 24 && dot->speed <= threshold) {
            threshold -= 24;
            dot->size--;
        }
        if (!reduce_effects || g_Stage->scroll.back == (i & 1)) {
            Draw_Dot(left, top, dot->size);
        }
    }
    Grcg_Off();
}

static void M_BitClear(const int bit)
{
    const ENTITY_POINT position =
        m_Marisa.bit_position[g_Stage->scroll.front][bit];
    const CLIP *const clip = g_Clip;
    int16_t first = Math16_Wrap((int32_t)position.y - clip->top);
    first = first > 0 ? first : 0;
    int16_t last =
        Math16_Wrap((int32_t)Math16_Wrap((int32_t)position.y + 32) - clip->top);
    const int16_t height = clip->bottom - clip->top;
    last = last < height ? last : height;
    const int16_t rows = Math16_Wrap((int32_t)last - first);
    if (rows < 0) {
        return;
    }
    ASSERT(clip->top >= 0 && clip->bottom < VIDEO_HEIGHT);
    ASSERT(first <= height);
    const uint32_t base = (clip->top + first) * 80;
    const int16_t left = position.x / 8 - (position.x % 8 < 0);
    uint16_t offset = (uint16_t)((int32_t)rows * 80 + left);
    uint8_t *const page = g_Video.pages[g_Video.drawing_page];
    for (;;) {
        for (unsigned col = 0; col < 6; col++) {
            // Charging aliases the contiguous B/R/G address ranges.
            const uint32_t address = (base + offset) % VIDEO_PLANE_SIZE;
            memset(page + address * 8, 0, 8);
            offset++;
        }
        // The byte-box loop stops on subtraction borrow, not a row count.
        if (offset < 86) {
            break;
        }
        offset -= 86;
    }
}

static void M_BodyHit(BOSS_STATE *const boss)
{
    MARISA *const marisa = &m_Marisa;
    int damage;

    if (boss->defeated != 0) {
        return;
    }
    if ((damage = M_RawHit(
             (marisa->position.x + 24), (marisa->position.y + 24), 48, 48))
        != 0) {
        if (marisa->bit_state_sum >= 8) {
            SoundEffects_Play(SOUND_EFFECT_HIT);
            boss->hit_flash = 1;
            boss->damage = Math16_Wrap(
                (int32_t)boss->damage + marisa->damage_enable * damage);
        } else {
            SoundEffects_Play(SOUND_EFFECT_BIT_BLOCK);
        }
        if (boss->damage >= 900) {
            boss->defeated = 1;
            Score_AddPoints(20000);
            Player_BeginInvincibility(BOSS_DEFEAT_INVINCIBILITY_FRAMES);
        }
    }
    if ((g_Player->position[g_Stage->scroll.front].x
         > (marisa->position.x - 16))
        && (g_Player->position[g_Stage->scroll.front].x
            < (marisa->position.x + 48))
        && (g_Player->position[g_Stage->scroll.front].y
            > (marisa->position.y - 16))
        && (g_Player->position[g_Stage->scroll.front].y
            < (marisa->position.y + 48))) {
        Player_Hit();
    }
}

static void M_BitHits(void)
{
    MARISA *const marisa = &m_Marisa;
    int i;

    m_Marisa.bomb_mask = 3;
    for (i = 0; i < MARISA_BIT_COUNT; i++) {
        if (marisa->bit_state[i] != 0) {
            continue;
        }
        if (M_RawHit(((*M_BitX(i)) + 4), (*M_BitY(i)), 24, 32)) {
            marisa->bit_hit[i] = 1;
            marisa->bit_hits[i]++;
            if (marisa->bit_hits[i] >= 110) {
                if ((Random_RingU8() % 80) >= g_Player->power) {
                    Items_Add(
                        ((*M_BitX(i)) + 8), ((*M_BitY(i)) + 8), ITEM_BIG_POWER);
                }
                SoundEffects_Play(SOUND_EFFECT_BOSS_HEAVY_HIT);
                marisa->bit_state[i] = 1;
                Score_AddPoints(5000);
            }
        }
        if ((g_Player->position[g_Stage->scroll.front].x > ((*M_BitX(i)) - 16))
            && (g_Player->position[g_Stage->scroll.front].x
                < ((*M_BitX(i)) + 16))
            && (g_Player->position[g_Stage->scroll.front].y
                > ((*M_BitY(i)) - 16))
            && (g_Player->position[g_Stage->scroll.front].y
                < ((*M_BitY(i)) + 16))) {
            Player_Hit();
        }
    }
    m_Marisa.bomb_mask = 1;
}

static int M_Defeat(BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    MARISA *const marisa = &m_Marisa;
    int zoom = 10;

    BossExplosion_Draw(
        patterns, boss->angle, (marisa->position.x + 40),
        (marisa->position.y + 40), marisa->defeat_frame);
    if (marisa->defeat_frame >= 24) {
        if (marisa->defeat_frame == 56) {
            boss->angle = 0x20;
        }
        BossExplosion_Draw(
            patterns, boss->angle, (marisa->position.x + 40),
            (marisa->position.y + 40), (marisa->defeat_frame - 24));
        if (marisa->defeat_frame >= 48) {
            if (marisa->defeat_frame == 80) {
                boss->angle = 0x00;
            }
            BossExplosion_Draw(
                patterns, boss->angle, (marisa->position.x + 40),
                (marisa->position.y + 40), (marisa->defeat_frame - 48));
            if ((marisa->defeat_frame >= 64)
                && ((marisa->defeat_frame & 0x0F) == 0)) {
                Sparks_Add(
                    g_Stage->scroll.back, (marisa->position.x + 48),
                    (marisa->position.y + 48), 136, 24, true);
            }
        }
    }
    zoom += ((marisa->defeat_frame - 32) >> 4);
    marisa->defeat_frame++;
    if (marisa->defeat_frame < 32) {
        M_Rect(patterns, marisa->position.x, marisa->position.y, boss->sprite);
        M_Rect(
            patterns, (marisa->position.x + 48), marisa->position.y,
            (boss->sprite + 1));
    } else {
        M_Zoom(patterns, marisa->position.x, marisa->position.y, zoom, 3);
    }
    if (marisa->defeat_frame >= 100) {
        marisa->defeat_frame = 0;
        boss->defeated = 3;
        return true;
    }
    return false;
}

static int M_BitDefeat(const PATTERN_BANK *const patterns, int bit)
{
    MARISA *const marisa = &m_Marisa;

    int patnum;

    Sparks_Add(
        g_Stage->scroll.back, ((*M_BitX(bit)) + 16), ((*M_BitY(bit)) + 16), 60,
        2, false);
    patnum = 10;
    patnum += (marisa->bit_flash[bit] / 6);
    marisa->bit_flash[bit]++;
    if (marisa->bit_flash[bit] >= 0x30) {
        marisa->bit_flash[bit] = 0;
        marisa->bit_state[bit] = 2;
        Video_AccessPage(g_Stage->scroll.front);
        Grcg_SetColor(0);
        M_BitClear(bit);
        Grcg_Off();
        Video_AccessPage(g_Stage->scroll.back);
        marisa->bit_hits[bit] = 0;
        return 1;
    }
    M_Rect(patterns, (*M_BitX(bit)), (*M_BitY(bit)), patnum);
    return 0;
}

static void M_DrawBits(const PATTERN_BANK *const patterns)
{
    MARISA *const marisa = &m_Marisa;
    int alive[MARISA_BIT_COUNT + 1];
    int count = 0;
    Grcg_SetColor(3);
    for (int i = 0; i < MARISA_BIT_COUNT; i++) {
        if (marisa->bit_state[i] == 0) {
            alive[count++] = i;
        }
    }
    if (count != 0) {
        alive[count] = alive[0];
    }
    for (int i = 0; i < count; i++) {
        Draw_Line(
            g_Clip, *M_BitX(alive[i]) + 16, *M_BitY(alive[i]) + 16,
            *M_BitX(alive[i + 1]) + 16, *M_BitY(alive[i + 1]) + 16);
    }
    Grcg_Off();
    for (int i = 0; i < MARISA_BIT_COUNT; i++) {
        if (marisa->bit_state[i] < 1) {
            const int16_t left = *M_BitX(i);
            const int16_t top = *M_BitY(i);
            if (marisa->bit_hit[i] != 0) {
                if (left > 0 && left < 416 && top >= 0 && top <= 368) {
                    Draw_SpritePlane(
                        M_Pattern(patterns, i + 138), left, top, 0, 0xffc0);
                }
                marisa->bit_hit[i] = 0;
            } else {
                M_Rect(patterns, left, top, i + 138);
            }
        }
    }
}

static void M_Cast(BOSS_STATE *const boss)
{
    if (boss->phase_frame == 50) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 130;
    } else if (boss->phase_frame == 100) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
        boss->sprite = 132;
    } else if (boss->phase_frame == 108) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
        boss->sprite = 134;
    } else if (boss->phase_frame == 116) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
        boss->sprite = 136;
    } else if (boss->phase_frame == 130) {
        boss->sprite = 128;
    }
}

static void M_Rank(const uint8_t rank)
{
    MARISA *const marisa = &m_Marisa;
    if (rank != 0) {
        marisa->fan = BULLET_FAN5_MEDIUM_AIMED;
        marisa->second_fan = BULLET_FAN4_MEDIUM_AIMED;
        marisa->unused_fan = BULLET_FAN2_MEDIUM;
        marisa->edge_count = 4;
    } else {
        marisa->fan = BULLET_FAN5_WIDE_AIMED;
        marisa->second_fan = BULLET_FAN4_WIDE_AIMED;
        marisa->unused_fan = BULLET_SINGLE;
        marisa->edge_count = 2;
    }
}

static void M_SpinPellets(BOSS_STATE *const boss, const uint8_t rank)
{
    MARISA *const marisa = &m_Marisa;
    if (boss->phase_frame < 50) {
        return;
    }
    M_Cast(boss);
    if (boss->phase_frame == 50) {
        marisa->double_angle = 0x18;
    } else if (boss->phase_frame == 130) {
        boss->phase_frame = 0;
        marisa->double_fire = 0;
    }
    if ((boss->phase_frame > 100) && ((boss->phase_frame % 3) == 0)) {
        if (marisa->double_fire != 0) {
            Bullets_AddPellets(
                g_Stage->scroll.back, (marisa->position.x + 36),
                (marisa->position.y + 64), marisa->double_angle, BULLET_SINGLE,
                60);
            Bullets_AddPellets(
                g_Stage->scroll.back, (marisa->position.x + 52),
                (marisa->position.y + 64), marisa->double_angle, BULLET_SINGLE,
                60);
        } else {
            Bullets_AddPellets(
                g_Stage->scroll.back, (marisa->position.x + 44),
                (marisa->position.y + 64), marisa->double_angle, BULLET_SINGLE,
                50);
        }
        marisa->double_angle += 0x0A;
    }
}

static void M_AimedFans(BOSS_STATE *const boss, const uint8_t rank)
{
    MARISA *const marisa = &m_Marisa;
    if (boss->phase_frame < 50) {
        return;
    }
    if (boss->phase_frame == 50) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 130;
    }
    if ((boss->phase_frame % 24) == 0) {
        Bullets_AddPellets(
            g_Stage->scroll.back, (marisa->position.x + 64),
            (marisa->position.y + 16), 0x00, marisa->fan, 54);
        Bullets_AddPellets(
            g_Stage->scroll.back, (marisa->position.x + 64),
            (marisa->position.y + 16), 0x00, marisa->second_fan, 60);
    }
    if (boss->phase_frame > 192) {
        boss->phase_frame = 0;
        boss->sprite = 128;
    }
}

static void M_DriftStars(BOSS_STATE *const boss, const uint8_t rank)
{
    MARISA *const marisa = &m_Marisa;
    if (boss->phase_frame < 50) {
        return;
    }
    M_Cast(boss);
    if (boss->phase_frame == 50) {
        marisa->drift_angle = Random_RingU8();
        Bullets_SetMotion(marisa->drift_toggle, 2);
        Bullets_SetTurnLimit(128);
        marisa->drift_toggle *= -1;
    } else if (boss->phase_frame == 130) {
        boss->phase_frame = 0;
    }
    if ((boss->phase_frame >= 100) && ((boss->phase_frame & 3) == 0)) {
        Bullets_AddSprites(
            g_Stage->scroll.back, (marisa->position.x + 44),
            (marisa->position.y + 64), marisa->drift_angle, BULLET_MOTION_DRIFT,
            85, 40);
        marisa->drift_angle += 0x2B;
    }
}

static void M_MoveBits(void)
{
    MARISA *const marisa = &m_Marisa;
    int i;

    for (i = 0; i < MARISA_BIT_COUNT; i++) {
        if (marisa->bit_state[i] == 0) {
            marisa->bit_angle[i] += marisa->bit_speed[i];
            (*M_BitX(i)) =
                (Math16_PolarX(
                     marisa->position.x, marisa->bit_radius[i],
                     marisa->bit_angle[i])
                 + 32);
            (*M_BitY(i)) =
                (Math16_PolarY(
                     marisa->position.y, marisa->bit_radius[i],
                     marisa->bit_angle[i])
                 + 32);
        }
    }
}

static void M_OpenBits(BOSS_STATE *const boss, const uint8_t rank)
{
    MARISA *const marisa = &m_Marisa;
    int i;
    int16_t left;
    int16_t top;

    if (boss->phase_frame < 100) {
        return;
    }
    if (boss->phase_frame == 100) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 130;
    } else if (boss->phase_frame == 130) {
        left = (marisa->position.x + 32);
        top = (marisa->position.y + 32);
        for (i = 0; i < MARISA_BIT_COUNT; i++) {
            marisa->bit_state[i] = 0;
            marisa->bit_angle[i] = (i << 6);
            marisa->bit_radius[i] = 8;
            marisa->bit_speed[i] = 2;
            marisa->bit_position[0][i].x = left;
            marisa->bit_position[1][i].x = left;
            marisa->bit_position[0][i].y = top;
            marisa->bit_position[1][i].y = top;
        }
    } else if (boss->phase_frame == 154) {
        boss->sprite = 128;
        boss->phase_frame = 0;
    }
    if (boss->phase_frame > 130) {
        for (i = 0; i < MARISA_BIT_COUNT; i++) {
            marisa->bit_radius[i] += 2;
        }
    }
}

static void M_Orbit(BOSS_STATE *const boss, const uint8_t rank)
{
    MARISA *const marisa = &m_Marisa;
    int bit;

    if (boss->phase_frame < 10) {
        return;
    }
    if (boss->phase_frame == 10) {
        marisa->orbit_direction =
            ((marisa->position.x + 32) < g_Player->current.x) ? 1 : -1;
        marisa->orbit_center.x = marisa->position.x;
        marisa->orbit_center.y = (marisa->position.y + 112);
        marisa->orbit_angle = -0x40;
    }
    marisa->orbit_angle += marisa->orbit_direction;
    M_BossPosition(boss)->x =
        Math16_PolarX(marisa->orbit_center.x, 0x70, marisa->orbit_angle);
    M_BossPosition(boss)->y =
        Math16_PolarY(marisa->orbit_center.y, 0x70, marisa->orbit_angle);
    marisa->position.x = M_BossPosition(boss)->x;
    marisa->position.y = M_BossPosition(boss)->y;
    if ((boss->phase_frame & 0x0F) == 0) {
        bit = Random_RingU8And(MARISA_BIT_COUNT - 1);
        if (marisa->bit_state[bit] == 0) {
            Bullets_AddPellets(
                g_Stage->scroll.back, ((*M_BitX(bit)) + 12),
                ((*M_BitY(bit)) + 12), 0x00, BULLET_AIMED, 80);
        }
        if ((marisa->bit_state_sum == 8) && (bit < 1)) {
            Bullets_AddPellets(
                g_Stage->scroll.back, (marisa->position.x + 64),
                (marisa->position.y + 16), 0x00, BULLET_FAN3_AIMED, 80);
        }
    }
    if (boss->phase_frame >= 266) {
        boss->phase_frame = 0;
    }
}

static void M_CastAt(BOSS_STATE *const boss, int frame)
{
    if (boss->phase_frame < frame) {
        return;
    }
    if (boss->phase_frame == frame) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 130;
    } else if ((frame + 30) == boss->phase_frame) {
        boss->sprite = 130;
    } else if ((frame + 36) == boss->phase_frame) {
        boss->sprite = 132;
    } else if ((frame + 42) == boss->phase_frame) {
        boss->sprite = 134;
    } else if ((frame + 50) == boss->phase_frame) {
        boss->sprite = 136;
    } else if ((frame + 60) == boss->phase_frame) {
        boss->sprite = 128;
    }
}

static void M_Expand(BOSS_STATE *const boss, const uint8_t rank)
{
    MARISA *const marisa = &m_Marisa;
    int i;

    if (boss->phase_frame < 50) {
        return;
    }
    if (boss->phase_frame < 280) {
        for (i = 0; i < MARISA_BIT_COUNT; i++) {
            marisa->bit_radius[i]++;
        }
    } else if (boss->phase_frame < 510) {
        for (i = 0; i < MARISA_BIT_COUNT; i++) {
            marisa->bit_radius[i]--;
        }
    } else {
        boss->phase_frame = 0;
        boss->sprite = 128;
    }
    M_CastAt(boss, 220);
    if (boss->phase_frame == 220) {
        marisa->expand_angle = 0;
    }
    if (marisa->bit_state_sum == 8) {
        boss->sprite = 130;
    }
    if (((boss->phase_frame >= 250) && (boss->phase_frame < 270))
        || (marisa->bit_state_sum == 8)) {
        if (marisa->bit_state_sum != 8) {
            marisa->expand_angle += 0x0D;
            if (rank >= (boss->phase_frame & 1)) {
                Bullets_AddPellets(
                    g_Stage->scroll.back, (marisa->position.x + 44),
                    (marisa->position.y + 64), marisa->expand_angle,
                    BULLET_SINGLE, 50);
            }
        } else {
            marisa->expand_angle += 0x0A;
            Bullets_AddPellets(
                g_Stage->scroll.back, (marisa->position.x + 64),
                (marisa->position.y + 16), marisa->expand_angle, BULLET_SINGLE,
                60);
            if (boss->phase_frame > 270) {
                boss->phase_frame = 0;
                boss->sprite = 128;
            }
        }
    }
}

static void M_SplitRadii(BOSS_STATE *const boss, const uint8_t rank)
{
    MARISA *const marisa = &m_Marisa;
    int16_t top;
    int i;
    int16_t left;

    if (boss->phase_frame == 10) {
        M_BitSpeed(3, -2);
    }
    if (boss->phase_frame < 50) {
        return;
    }
    if (boss->phase_frame < 130) {
        for (i = 0; i < MARISA_BIT_COUNT; i += 2) {
            marisa->bit_radius[i] += 2;
        }
    } else if (boss->phase_frame < 260) {
    } else if (boss->phase_frame < 340) {
        for (i = 0; i < MARISA_BIT_COUNT; i += 2) {
            marisa->bit_radius[i] -= 2;
        }
        if (rank != 0) {
            for (i = 1; i < MARISA_BIT_COUNT; i += 2) {
                marisa->bit_radius[i]++;
            }
        }
    } else if (boss->phase_frame < 420) {
        if (rank != 0) {
            for (i = 1; i < MARISA_BIT_COUNT; i += 2) {
                marisa->bit_radius[i]--;
            }
        }
    } else {
        M_BitSpeed(2, -2);
        boss->phase_frame = 0;
        boss->sprite = 128;
    }
    M_CastAt(boss, 140);
    if (boss->phase_frame == 140) {
        marisa->spin_angle = 0;
    }
    if (marisa->bit_state_sum == 8) {
        boss->sprite = 130;
        marisa->spin_angle -= (0x18 - rank);
        Bullets_AddSprites(
            g_Stage->scroll.back, (marisa->position.x + 64),
            (marisa->position.y + 16), marisa->spin_angle, BULLET_MOTION_SINGLE,
            85, 60);
    } else if (
        (boss->phase_frame >= 150) && (boss->phase_frame < 250)
        && ((boss->phase_frame & 3) == 0)) {
        marisa->spin_angle += 3;
        SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
        for (i = 0; i < MARISA_BIT_COUNT; i += 2) {
            if (marisa->bit_state[i] == 0) {
                left = (*M_BitX(i));
                top = (*M_BitY(i));
                if ((left > 0) && (left < 0x1A0) && (top >= 0)
                    && (top <= 0x170)) {
                    Bullets_AddPellets(
                        g_Stage->scroll.back, (left + 12), (top + 12),
                        (marisa->bit_angle[i] + marisa->spin_angle),
                        BULLET_SINGLE, 70);
                }
            }
        }
    }
}

static void M_Detonate(BOSS_STATE *const boss, const uint8_t rank)
{
    MARISA *const marisa = &m_Marisa;
    int16_t top;
    int angle;
    int i;
    int16_t left;

    if (boss->phase_frame == 10) {
        M_BitSpeed(-3, -3);
    }
    if (marisa->bit_state_sum == 8) {
        boss->sprite = 128;
        boss->phase_frame = 0;
    }
    if (boss->phase_frame < 50) {
        return;
    }
    if (boss->phase_frame == 50) {
        M_BitSpeed(-4, -4);
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 130;
    } else if (boss->phase_frame == 100) {
        SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        for (i = 0; i < MARISA_BIT_COUNT; i++) {
            if (marisa->bit_state[i] == 0) {
                left = ((*M_BitX(i)) + 12);
                top = ((*M_BitY(i)) + 12);
                for (angle = -0x40; angle < 0x40; angle += 0x10) {
                    Bullets_AddPellets(
                        g_Stage->scroll.back, left, top,
                        (marisa->bit_angle[i] + angle), BULLET_SINGLE, 70);
                }
            }
        }
    } else if (boss->phase_frame == 110) {
        SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        for (i = 0; i < MARISA_BIT_COUNT; i++) {
            if (marisa->bit_state[i] == 0) {
                left = ((*M_BitX(i)) + 12);
                top = ((*M_BitY(i)) + 12);
                for (angle = -0x46; angle < 0x46; angle += 0x0C) {
                    Bullets_AddPellets(
                        g_Stage->scroll.back, left, top,
                        (marisa->bit_angle[i] + angle), BULLET_SINGLE,
                        (Random_RingU8And(0x1F) + 16));
                }
                marisa->bit_state[i] = 1;
            }
        }
    }
}

static void M_Spin(BOSS_STATE *const boss, const uint8_t rank)
{
    MARISA *const marisa = &m_Marisa;
    int16_t top;
    int16_t speed;
    int i;
    int16_t left;

    if (boss->phase_frame < 50) {
        return;
    }
    if (boss->phase_frame >= 150) {
        M_CastAt(boss, 120);
    }
    if (boss->phase_frame == 50) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 130;
        marisa->ring_toggle = 0;
        marisa->bit_spin[0] = 0;
        marisa->bit_spin[1] = 0;
        marisa->bit_spin[2] = 0;
        marisa->bit_spin[3] = 0;
    } else if (boss->phase_frame == 300) {
        boss->phase_frame = 0;
        marisa->bit_speed[0] *= -1;
        marisa->bit_speed[1] *= -1;
        marisa->bit_speed[2] *= -1;
        marisa->bit_speed[3] *= -1;
    }
    if ((boss->phase_frame > 70) && ((boss->phase_frame % 5) == 0)) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
        for (i = 0; i < MARISA_BIT_COUNT; i++) {
            if (marisa->bit_state[i] == 0) {
                left = ((*M_BitX(i)) + 12);
                top = ((*M_BitY(i)) + 12);
                speed = ((boss->phase_frame >> 2) + 25);
                Bullets_AddPellets(
                    g_Stage->scroll.back, left, top,
                    (marisa->bit_angle[i] + marisa->bit_spin[i]), BULLET_SINGLE,
                    speed);
                marisa->bit_spin[i] += (marisa->bit_speed[i] * 2);
            }
        }
        if ((marisa->bit_state_sum == 8) && ((marisa->ring_toggle & 1) == 0)) {
            Bullets_AddPellets(
                g_Stage->scroll.back, (marisa->position.x + 44),
                (marisa->position.y + 44), boss->phase_frame, BULLET_RING16,
                50);
        }
        marisa->ring_toggle++;
    }
}

static void M_EdgeStars(BOSS_STATE *const boss, const uint8_t rank)
{
    MARISA *const marisa = &m_Marisa;
    int i;
    uint8_t angle;
    int16_t edge_left;

    if (boss->phase_frame < 50) {
        return;
    }
    if (boss->phase_frame >= 150) {
        M_CastAt(boss, 120);
    }
    if (boss->phase_frame == 50) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 130;
        Bullets_SetMotion(marisa->edge_direction, 0);
        Bullets_SetTurnLimit(64);
        marisa->edge_direction *= -1;
        M_BitSpeed(2, -2);
    } else if (boss->phase_frame == 70) {
        M_BitSpeed(3, -3);
    } else if (boss->phase_frame == 80) {
        M_BitSpeed(4, -4);
    } else if (boss->phase_frame == 200) {
        M_BitSpeed(2, -2);
        boss->phase_frame = 0;
    }
    if ((boss->phase_frame > 70) && ((boss->phase_frame % 14) == 0)) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
        if (boss->phase_frame <= 120) {
            for (i = 0; i < MARISA_BIT_COUNT; i++) {
                if (marisa->bit_state[i] == 0) {
                    M_Star(
                        ((*M_BitX(i)) + 8), ((*M_BitY(i)) + 8), -0x40,
                        BULLET_MOTION_SINGLE);
                }
            }
            M_Star(
                (marisa->position.x + 40), (marisa->position.y + 40), -0x40,
                BULLET_MOTION_SINGLE);
        } else {
            if (marisa->edge_direction == 1) {
                edge_left = 0x190;
            } else {
                edge_left = 0x20;
            }
            for (i = 0; marisa->edge_count > i; i++) {
                angle =
                    (Random_RingU8And(7) + (marisa->edge_direction * 16)
                     + 0x40);
                const int16_t first_left = Random_RingU16() % 368 + 32;
                const int16_t first_speed = Random_RingU8And(31) + 16;
                Bullets_AddSprites(
                    g_Stage->scroll.back, first_left, 16, angle,
                    BULLET_MOTION_SINGLE, 85, first_speed);
                const int16_t second_top = Random_RingU16() % 320 + 16;
                const int16_t second_speed = Random_RingU8And(31) + 16;
                Bullets_AddSprites(
                    g_Stage->scroll.back, edge_left, second_top, angle,
                    BULLET_MOTION_SINGLE, 85, second_speed);
            }
        }
        if (marisa->bit_state_sum == 8) {
            for (i = 0; i < MARISA_BIT_COUNT; i++) {
                M_Star(
                    (marisa->position.x + 40), (marisa->position.y + 40),
                    ((i << 6) + (boss->phase_frame * 2)),
                    BULLET_MOTION_DRIFT_CHASE);
            }
        }
    }
}

static void M_Wander(BOSS_STATE *const boss)
{
    MARISA *const marisa = &m_Marisa;
    if (boss->phase_frame == 1) {
        return;
    }
    if (boss->phase_frame == 2) {
        marisa->move_x =
            ((marisa->position.x + 32) < g_Player->current.x) ? 1 : -1;
        marisa->move_y = (marisa->position.y < 72) ? 1
            : (marisa->position.y > 108)           ? -1
                                         : (1 - (Random_RingU8() % 3));
    }
    if (boss->phase_frame < 50) {
        M_BossPosition(boss)->x += marisa->move_x;
        M_BossPosition(boss)->y += marisa->move_y;
        marisa->position.x = M_BossPosition(boss)->x;
        marisa->position.y = M_BossPosition(boss)->y;
    }
}

static void Marisa_Init(void)
{
    memset(&m_Marisa, 0, sizeof(m_Marisa));
    M_DotsInit();
    m_Marisa.drift_toggle = 2;
    m_Marisa.edge_direction = 1;
    m_Marisa.bomb_mask = 1;
}

static bool Marisa_MidbossInvalidate(BOSS_STATE *const boss)
{
    m_Marisa.midboss_page = g_Stage->scroll.back;
    ENTITY_POINT *const position = M_Position(boss);
    Background_Mark(g_Stage->scroll.line, position->x, position->y, 64, 64);
    *position = boss->position[g_Stage->scroll.front];
    return m_Marisa.midboss_active;
}

static void Marisa_MidbossUpdate(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns,
    const uint8_t rank)
{
    MARISA *const marisa = &m_Marisa;
    Shots_SetTarget(
        (ENTITY_POINT) { Math16_Wrap((int32_t)marisa->position.x + 24),
                         Math16_Wrap((int32_t)marisa->position.y + 24) });
    boss->phase_frame = Math16_Wrap((int32_t)boss->phase_frame + 1);
    if (boss->phase_frame == 1) {
        boss->position[0].x = marisa->direction == 0 ? 64 : 320;
        boss->sprite = marisa->direction == 0 ? 150 : 149;
        boss->position[1].x = boss->position[0].x;
        boss->position[0].y = -16;
        boss->position[1].y = -16;
        marisa->midboss_page = 0;
        boss->damage = 0;
        boss->defeated = 0;
        boss->hit_flash = 0;
        marisa->phase = MARISA_MIDBOSS_ENTER;
        marisa->direction = marisa->direction == 0 ? 1 : -1;
        marisa->midboss_patterns_done = 0;
        marisa->midboss_pattern = MARISA_MIDBOSS_PELLETS;
        marisa->midboss_active = true;
    }
    ENTITY_POINT *const position = M_Position(boss);
    marisa->midboss_bullet_top = Math16_Wrap((int32_t)position->y + 48);
    marisa->position.y = position->y;
    if (boss->defeated == 0) {
        if (marisa->phase == MARISA_MIDBOSS_ENTER) {
            position->x =
                Math16_Wrap((int32_t)position->x + marisa->direction * 8);
            position->y = Math16_Wrap((int32_t)position->y + 2);
            marisa->position = *position;
            if ((boss->phase_frame & 7) == 0) {
                SoundEffects_Play(SOUND_EFFECT_PILLAR_FALL);
                M_Pellets(marisa->midboss_bullet_top, 64, BULLET_SINGLE, 80);
            }
            if (boss->phase_frame >= 32) {
                marisa->phase = MARISA_MIDBOSS_TRAVERSE;
                M_Turn(boss);
            }
        } else if (
            marisa->phase == MARISA_MIDBOSS_TRAVERSE
            && boss->phase_frame > 50) {
            boss->sprite = marisa->direction == -1 ? 149 : 150;
            position->x =
                Math16_Wrap((int32_t)position->x + marisa->direction * 4);
            marisa->position.x = position->x;
            M_MidbossFire(boss, rank);
            if (marisa->midboss_patterns_done < 12) {
                if (boss->phase_frame >= 114) {
                    M_Turn(boss);
                    marisa->midboss_patterns_done++;
                }
            } else if (marisa->position.x <= 0 || marisa->position.x >= 384) {
                boss->phase_frame = 0;
                marisa->direction = -1;
                Stage_SetMidbossRow(1632);
                marisa->midboss_active = false;
                return;
            }
        }
    }
    if (boss->defeated != 0) {
        M_MidbossDefeat(boss, patterns);
        return;
    }
    M_MidbossHit(boss);
    M_MarisaRender(boss, patterns);
}

void Marisa_SelectPatterns(
    PATTERN_BANK *const patterns, const MARISA_ASSETS *const assets,
    const bool combat)
{
    if (assets == nullptr) {
        return;
    }
    const PATTERN_BANK *const bank = combat ? &assets->combat : &assets->summon;
    Boss_SelectPatterns(
        patterns, bank,
        combat
            ? (assets->has_combat_palette ? assets->combat_palette : nullptr)
            : (assets->has_summon_palette ? assets->summon_palette : nullptr));
}

void Marisa_Place(BOSS_STATE *const boss)
{
    boss->position[0] = boss->position[1] = (ENTITY_POINT) { 176, 64 };
    m_Marisa.position = boss->position[0];
    boss->sprite = 128;
}

static void Marisa_DrawBody(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    if (boss->hit_flash != 0) {
        boss->hit_flash = 0;
        BossScene_DrawFlash(patterns, boss->sprite, m_Marisa.position, 2, 96);
        return;
    }
    M_Rect(patterns, m_Marisa.position.x, m_Marisa.position.y, boss->sprite);
    M_Rect(
        patterns, Math16_Wrap((int32_t)m_Marisa.position.x + 48),
        m_Marisa.position.y, boss->sprite + 1);
}

void Marisa_DrawScene(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    Marisa_DrawBody(boss, patterns);
    BossScene_DrawPlayer(patterns);
}

static void Marisa_Begin(BOSS_STATE *const boss, const uint8_t rank)
{
    MARISA *const marisa = &m_Marisa;
    Background_SetMode(TILE_DISABLED);
    Shots_FreeAll();
    Boss_ResetDamage(&boss->damage, 900);
    boss->defeated = 0;
    boss->phase_frame = 0;
    boss->hit_flash = 0;
    marisa->phase = MARISA_OPENING;
    marisa->damage_enable = 0;
    for (int i = 0; i < MARISA_BIT_COUNT; i++) {
        marisa->bit_hits[i] = 0;
        marisa->bit_state[i] = 2;
        marisa->bit_hit[i] = 0;
    }
    marisa->double_fire = 1;
    marisa->background_color = 9;
    marisa->direction = 0;
    marisa->color_index = 0;
    marisa->pattern = MARISA_PATTERN_OPEN_ORBS;
    marisa->pattern_cycle = 0;
    marisa->bit_patterns_seen = 0;
    boss->angle = 224;
    M_Rank(rank);
}

static void Marisa_Invalidate(BOSS_STATE *const boss, const bool reduce_effects)
{
    const CLIP full = { 0, 0, 639, 399 };
    Grcg_SetColor(0);
    Draw_BoxFill(&full, 32, 16, 415, 383);
    M_DotsInvalidate(reduce_effects);
    *M_BossPosition(boss) = boss->position[g_Stage->scroll.front];
    for (int i = 0; i < MARISA_BIT_COUNT; i++) {
        if (m_Marisa.bit_state[i] < 2) {
            m_Marisa.bit_position[g_Stage->scroll.back][i] =
                m_Marisa.bit_position[g_Stage->scroll.front][i];
        }
    }
    Grcg_Off();
}

static BOSS_RESULT Marisa_Update(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns,
    const uint8_t rank, const bool reduce_effects)
{
    MARISA *const marisa = &m_Marisa;
    int i;
    int16_t left;

    for (i = 0, marisa->bit_state_sum = 0; i < MARISA_BIT_COUNT; i++) {
        marisa->bit_state_sum += marisa->bit_state[i];
    }
    if (marisa->bit_state_sum == 8) {
        Shots_SetTarget((ENTITY_POINT) { marisa->position.x + 40,
                                         marisa->position.y + 40 });
    } else {
        Shots_SetTarget((ENTITY_POINT) { -1, -1 });
    }
    boss->phase_frame = Math16_Wrap((int32_t)boss->phase_frame + 1);
    if ((g_Stage->world.frame & 1)
        && (!reduce_effects || (g_Stage->world.frame & 3))) {
        left = ((Random_RingU16() % 384) + 32);
        M_DotAdd(left, 16, (((0xE0 - left) / 3) + 0x40));
    }
    if ((g_Stage->world.frame & 0x1F) == 0) {
        marisa->color_index++;
        if (marisa->color_index >= 6) {
            marisa->color_index = 0;
        }
        marisa->background_color = m_BackgroundColors[marisa->color_index];
    }
    M_DotsUpdate(reduce_effects);
    if (boss->defeated == 0) {
        if (marisa->phase == MARISA_OPENING) {
            M_SpinPellets(boss, rank);
            if (boss->phase_frame == 0) {
                marisa->phase = MARISA_OPEN_ORBS;
            }
        } else if (marisa->phase == MARISA_OPEN_ORBS) {
            M_OpenBits(boss, rank);
            if (boss->phase_frame == 0) {
                marisa->phase = MARISA_COMBAT;
                marisa->pattern = MARISA_PATTERN_ORBIT;
                marisa->pattern_rounds = 0;
            }
        } else if (marisa->phase == MARISA_COMBAT) {
            static const MARISA_ATTACK attacks[] = {
                { M_DriftStars, true },  { M_AimedFans, true },
                { M_SpinPellets, true }, { M_OpenBits, false },
                { M_Orbit, false },      { M_Expand, true },
                { M_Spin, true },        { M_EdgeStars, true },
                { M_SplitRadii, true },  { M_Detonate, true },
            };
            const int attack = marisa->pattern - MARISA_PATTERN_DRIFT;
            if (attack >= 0 && attack < (int)ARRAY_SIZE(attacks)) {
                attacks[attack].update(boss, rank);
                if (attacks[attack].move) {
                    M_Wander(boss);
                }
            } else if (marisa->pattern == MARISA_PATTERN_CHOOSE) {
                M_Wander(boss);
                if (boss->phase_frame > 30) {
                    if (marisa->bit_state_sum < 8) {
                        if (marisa->pattern_rounds >= 4) {
                            marisa->pattern = MARISA_PATTERN_DETONATE;
                        } else {
                            marisa->pattern = ((Random_RingU8() % 5) + 1);
                            marisa->pattern_rounds++;
                        }
                    } else if (marisa->pattern_cycle >= 2) {
                        marisa->pattern_rounds = 0;
                        marisa->pattern = MARISA_PATTERN_OPEN_ORBS;
                        marisa->pattern_cycle = 0;
                        marisa->bit_patterns_seen++;
                        if (marisa->bit_patterns_seen >= 2) {
                            marisa->damage_enable = 1;
                        }
                        if (marisa->bit_patterns_seen >= 7) {
                            boss->defeated = 1;
                        }
                    } else {
                        marisa->pattern_cycle++;
                        marisa->pattern =
                            (int8_t)(uint8_t)(0xFF - (Random_RingU8() % 3));
                    }
                    boss->phase_frame = 1;
                }
            }
            if (boss->phase_frame == 0) {
                marisa->pattern = MARISA_PATTERN_CHOOSE;
            }
        }
    }
    M_BodyHit(boss);
    M_BitHits();
    if (boss->defeated != 0) {
        if (M_Defeat(boss, patterns)) {
            return BOSS_FINISHED;
        }
    } else {
        Marisa_DrawBody(boss, patterns);
    }
    for (i = 0; i < MARISA_BIT_COUNT; i++) {
        if (marisa->bit_state[i] == 1) {
            M_BitDefeat(patterns, i);
        }
    }
    M_MoveBits();
    M_DrawBits(patterns);
    return BOSS_RUNNING;
}

// Stage 4's pre-fight dialogue reveals Marisa and Mima together, so it
// overrides the common talk flow outright instead of using place/draw_scene.
typedef enum {
    MARISA_TALK_ENTER,
    MARISA_TALK_INTRO,
    MARISA_TALK_WAIT,
    MARISA_TALK_APPEARANCE,
    MARISA_TALK_REVEAL,
    MARISA_TALK_WHITE_OUT,
    MARISA_TALK_WHITE_IN,
    MARISA_TALK_SCRIPT,
    MARISA_TALK_POST,
    MARISA_TALK_BEFORE_CONTINUE,
    MARISA_TALK_GENERIC,
    MARISA_TALK_DONE,
} MARISA_TALK_PHASE;

typedef struct {
    DIALOGUE_SEQUENCE sequence;
    DIALOGUE_SUMMON summon;
    DIALOGUE_RUNTIME runtime;
    WAIT wait;
    MARISA_TALK_PHASE phase;
} MARISA_TALK;

static MARISA_TALK m_Talk;

static bool M_TalkStep(const TH02_INPUT *const input)
{
    const bool ready = DialogueSequence_Update(&m_Talk.sequence, input);
    Stage_SetKeys(m_Talk.runtime.keys);
    return ready;
}

static void M_TalkFade(
    const PALETTE_FADE_KIND kind, const MARISA_TALK_PHASE phase)
{
    Wait_Fade(&m_Talk.wait, kind, 1);
    m_Talk.phase = phase;
}

static bool M_TalkFadeReady(void)
{
    return Wait_Ready(&m_Talk.wait, nullptr, 0);
}

static void M_TalkBegin(
    BOSS_STATE *const boss, PATTERN_BANK *const patterns,
    const BOSS_ASSETS *const assets, const BOSS_MUSIC *const music,
    const TH02_SESSION *const session, const bool post)
{
    (void)boss;
    (void)music;
    const DIALOGUE_SCRIPTS *const scripts = &assets->scripts;
    DialogueSequence_BeginPart(
        &m_Talk.sequence, patterns, &assets->stages[session->stage],
        &m_Talk.runtime, post ? scripts->faces[7] : scripts->stage4_intro,
        post ? scripts->counts[7] : 0);
    m_Talk.phase = post ? MARISA_TALK_POST : MARISA_TALK_ENTER;
}

static bool M_TalkUpdate(
    BOSS_STATE *const boss, PATTERN_BANK *const patterns,
    const BOSS_ASSETS *const assets, const BOSS_MUSIC *const music,
    const TH02_SESSION *const session, const bool post,
    const TH02_INPUT *const input)
{
    (void)post;
    const DIALOGUE_SCRIPTS *const scripts = &assets->scripts;
    DIALOGUE_SEQUENCE *const sequence = &m_Talk.sequence;
    while (true) {
        switch (m_Talk.phase) {
        case MARISA_TALK_ENTER:
            if (!M_TalkStep(input)) {
                return false;
            }
            Marisa_SelectPatterns(
                patterns,
                assets->marisa.combat.next != 0 ? &assets->marisa : nullptr,
                false);
            DialogueSequence_NextPart(sequence, scripts->stage4_intro, 8);
            m_Talk.phase = MARISA_TALK_INTRO;
            break;
        case MARISA_TALK_INTRO:
            if (!M_TalkStep(input)) {
                return false;
            }
            Wait_Delay(&m_Talk.wait, 10);
            m_Talk.phase = MARISA_TALK_WAIT;
            break;
        case MARISA_TALK_WAIT:
            if (!Wait_Ready(&m_Talk.wait, nullptr, 0)) {
                return false;
            }
            DialogueSummon_Begin(
                &m_Talk.summon, patterns, BossScene_DrawSummon);
            m_Talk.phase = MARISA_TALK_APPEARANCE;
            break;
        case MARISA_TALK_APPEARANCE:
            if (!DialogueSummon_Update(&m_Talk.summon)) {
                return false;
            }
            DialogueSequence_NextPart(
                sequence, scripts->faces[6], scripts->counts[6]);
            m_Talk.phase = MARISA_TALK_REVEAL;
            break;
        case MARISA_TALK_REVEAL:
            if (!M_TalkStep(input)) {
                return false;
            }
            Marisa_SelectPatterns(
                patterns,
                assets->marisa.combat.next != 0 ? &assets->marisa : nullptr,
                true);
            Marisa_Place(boss);
            M_TalkFade(PALETTE_WHITE_OUT, MARISA_TALK_WHITE_OUT);
            break;
        case MARISA_TALK_WHITE_OUT:
            if (!M_TalkFadeReady()) {
                return false;
            }
            Grcg_SetColor(0);
            Draw_BoxFill(g_Clip, 32, 16, 416, 320);
            Grcg_Off();
            Clip_SetPlayfield();
            Marisa_DrawScene(boss, patterns);
            SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
            SoundEffects_Update();
            BossScene_DrawBorder(patterns);
            M_TalkFade(PALETTE_WHITE_IN, MARISA_TALK_WHITE_IN);
            break;
        case MARISA_TALK_WHITE_IN:
            if (!M_TalkFadeReady()) {
                return false;
            }
            if (music->boss != nullptr) {
                music->boss(music->context);
            }
            DialogueSequence_NextPart(sequence, scripts->stage4_marisa, 11);
            m_Talk.phase = MARISA_TALK_SCRIPT;
            break;
        case MARISA_TALK_SCRIPT:
            if (!M_TalkStep(input)) {
                return false;
            }
            sequence->view.wipe(sequence->view.context);
            Video_Clear();
            Marisa_DrawScene(boss, patterns);
            Result_Ignore(GameplayMemory_CopyPage(g_Stage->scroll.front));
            Video_AccessPage(g_Stage->scroll.back);
            Video_ShowPage(g_Stage->scroll.front);
            m_Talk.phase = MARISA_TALK_DONE;
            return true;
        case MARISA_TALK_POST:
            if (!M_TalkStep(input)) {
                return false;
            }
            if (session->continues_used != 0) {
                DialogueSequence_NextPart(sequence, scripts->stage4_before, 3);
                m_Talk.phase = MARISA_TALK_BEFORE_CONTINUE;
            } else {
                m_Talk.runtime.box += 8;
                DialogueSequence_NextPart(
                    sequence, scripts->stage4_no_continue, 5);
                m_Talk.phase = MARISA_TALK_GENERIC;
            }
            break;
        case MARISA_TALK_BEFORE_CONTINUE:
            if (!M_TalkStep(input)) {
                return false;
            }
            DialogueRender_Continues(
                &sequence->render, m_Talk.runtime.box, scripts->digits,
                session->continues_used);
            DialogueSequence_NextPart(sequence, scripts->stage4_after, 5);
            m_Talk.phase = MARISA_TALK_GENERIC;
            break;
        case MARISA_TALK_GENERIC:
            if (!M_TalkStep(input)) {
                return false;
            }
            m_Talk.phase = MARISA_TALK_DONE;
            return true;
        case MARISA_TALK_DONE:
            return true;
        default:
            ASSERT(false);
            return false;
        }
    }
}

static void M_Begin(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const TH02_SESSION *const session)
{
    if (assets->stages[session->stage].boxes == nullptr) {
        Marisa_SelectPatterns(
            patterns,
            assets->marisa.combat.next != 0 ? &assets->marisa : nullptr, true);
        Marisa_Place(p_BossState);
    }
    Marisa_Begin(p_BossState, (uint8_t)session->rank);
}

static void M_Invalidate(const bool reduce_effects)
{
    Marisa_Invalidate(p_BossState, reduce_effects);
}

static BOSS_RESULT M_Update(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, TH02_SESSION *const session,
    const TH02_INPUT *const input)
{
    return Marisa_Update(
        p_BossState, patterns, (uint8_t)session->rank, session->reduce_effects);
}

static bool M_MidbossInvalidate(void)
{
    return Marisa_MidbossInvalidate(p_BossState);
}

static void M_MidbossUpdate(
    const PATTERN_BANK *const patterns, const uint8_t rank)
{
    Marisa_MidbossUpdate(p_BossState, patterns, rank);
}

const BOSS_STRATEGY g_BossMarisa = {
    .init = Marisa_Init,
    .begin = M_Begin,
    .invalidate = M_Invalidate,
    .update = M_Update,
    .post_dialogue = true,
    .midboss_invalidate = M_MidbossInvalidate,
    .midboss_update = M_MidbossUpdate,
    .talk = { .begin = M_TalkBegin, .update = M_TalkUpdate },
};
