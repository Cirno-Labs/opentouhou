#include "th02/background.h"
#include "th02/boss/mima.h"
#include "th02/boss/mima/assets.h"
#include "th02/boss/mima/private.h"
#include "th02/boss/private.h"
#include "th02/boss/scene.h"
#include "th02/boss/strategy.h"
#include "th02/bullets.h"
#include "th02/draw.h"
#include "th02/items.h"
#include "th02/palette.h"
#include "th02/player.h"
#include "th02/random.h"
#include "th02/score.h"
#include "th02/shots.h"
#include "th02/sound_effects.h"
#include "th02/stage.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

#define MIMA_SPHERE_COUNT 8

typedef enum {
    MIMA_APPEAR,
    MIMA_ATTACK_ONE,
    MIMA_CHANGE_TWO,
    MIMA_ATTACK_TWO,
    MIMA_CHANGE_THREE,
    MIMA_ATTACK_THREE,
    MIMA_CHANGE_FOUR,
    MIMA_ATTACK_FOUR,
    MIMA_FORM,
    MIMA_ATTACK_FINAL,
    MIMA_POST,
} MIMA_PHASE;

typedef struct {
    ENTITY_POINT shot_origin;
    ENTITY_POINT pellet_origin;
    ENTITY_POINT lower_origin;
    ENTITY_POINT ball_origin;
    int16_t damage_enable;
    int16_t phase;
    int16_t pattern;
    int16_t inactive_spheres;
    int16_t pattern_cycles;
    ENTITY_POINT sphere_position[2][MIMA_SPHERE_COUNT];
    int16_t sphere_active[MIMA_SPHERE_COUNT];
    uint8_t form, background_radius, disc_radius, ring_color, trail_color,
        disc_color, flash_cycle, beam_angle, beam_spread, beam_tone,
        orbit_burst_mode;
    ENTITY_POINT orbit_center;
    ENTITY_POINT aimed_velocity;
    ENTITY_POINT twist_velocity;
    ENTITY_POINT expand_center;
    int16_t orbit_radius;
    int16_t expand_radius;
    uint8_t orbit_angle, orbit_spinning, spiral_angle, spiral_speed, fan_angle,
        star_angle, cross_angle, expand_angle, ball_angle, cue_angle,
        cue_radius;
    int8_t orbit_direction;
    int8_t star_direction;
    int16_t damage_threshold;
    int16_t max_patterns;
    int16_t pattern_count;
    int16_t min_patterns;
    uint8_t effect_radius;
    uint8_t disc_base_radius;
    uint8_t disc_pulse;
    uint8_t background_angle;
    int16_t move_y;
    uint8_t ring;
    uint8_t groups[2];
    uint8_t ball_group;
    uint8_t angle_step;
    bool transition_started;
    bool ended;
} MIMA;

typedef void (*MIMA_ATTACK)(BOSS_STATE *boss, uint8_t rank);

static MIMA m_Mima;

static ENTITY_POINT *M_Position(BOSS_STATE *const boss)
{
    return &boss->position[g_Stage->scroll.back];
}

static ENTITY_POINT M_Aim(const int16_t x, const int16_t y, const int16_t speed)
{
    const MATH16_VECTOR v = Math16_Aim(
        (MATH16_VECTOR) { x, y },
        (MATH16_VECTOR) { g_Player->current.x, g_Player->current.y }, speed, 0);
    return (ENTITY_POINT) { v.x, v.y };
}

static void M_MoveToward(BOSS_STATE *const boss)
{
    ENTITY_POINT *const p = M_Position(boss);
    p->x = Math16_Wrap(
        (int32_t)p->x
        + (Math16_Wrap((int32_t)p->x + 64) < g_Player->current.x ? 2 : -2));
}

static void M_Star(const int delta)
{
    Bullets_AddSprites(
        g_Stage->scroll.back, m_Mima.lower_origin.x, m_Mima.lower_origin.y,
        m_Mima.star_angle + delta, BULLET_MOTION_BOUNCE_VERTICAL, 85, 80);
}

static void M_RandomPair(void)
{
    Bullets_AddSprites(
        g_Stage->scroll.back, m_Mima.ball_origin.x, m_Mima.ball_origin.y, 2,
        BULLET_RANDOM_SPEED, 86, 60);
    Bullets_AddPellets(
        g_Stage->scroll.back, m_Mima.ball_origin.x, m_Mima.ball_origin.y, 2,
        BULLET_RANDOM_SPEED, 60);
}

static void M_Palette(const uint8_t r, const uint8_t g, const uint8_t b)
{
    Palette_SetColor(0, r, g, b);
    Palette_Show();
}

static void M_Circles(const uint8_t color)
{
    Grcg_SetColor(color);
    for (int scale = 1; scale <= 4; scale *= 2) {
        Draw_Circle(
            m_Mima.ball_origin.x, m_Mima.ball_origin.y,
            m_Mima.cue_radius * scale);
    }
}

static void M_Ring(const uint8_t radius)
{
    for (int angle = 0; angle < 256; angle += 16) {
        const int16_t x = Math16_PolarX(224, radius, angle);
        const int16_t y = Math16_PolarY(200, radius, angle);
        if (x > 24 && x < 416 && y > 8 && y < 384) {
            Draw_Dot(x, y, (radius >> 6) + 1);
        }
    }
}

static void M_BodyHit(BOSS_STATE *const boss)
{
    if (boss->defeated != 0) {
        return;
    }
    const ENTITY_POINT p = *M_Position(boss);
    if (Boss_Hit(
            &boss->damage, Math16_Wrap((int32_t)p.x + 48),
            Math16_Wrap((int32_t)p.y + 32), 64, 64, m_Mima.damage_enable, 1)
        != 0) {
        boss->hit_flash = 1;
        if (boss->damage >= 6000) {
            SoundEffects_Play(SOUND_EFFECT_BOSS_HEAVY_HIT);
            boss->defeated = 1;
            Score_AddBonus(500000);
            Player_BeginInvincibility(BOSS_DEFEAT_INVINCIBILITY_FRAMES);
        }
    }
    const ENTITY_POINT player = g_Player->position[g_Stage->scroll.front];
    if (player.x > Math16_Wrap((int32_t)p.x + 16)
        && player.x < Math16_Wrap((int32_t)p.x + 112) && player.y > p.y
        && player.y < Math16_Wrap((int32_t)p.y + 80)) {
        Player_Hit();
    }
}

static void M_Spheres(const PATTERN_BANK *const patterns)
{
    m_Mima.inactive_spheres = 0;
    for (int i = 0; i < MIMA_SPHERE_COUNT; i++) {
        if (m_Mima.sphere_active[i] != 1) {
            m_Mima.inactive_spheres++;
            continue;
        }
        const ENTITY_POINT p = m_Mima.sphere_position[g_Stage->scroll.back][i];
        int16_t ignored = 0;
        Boss_Hit(&ignored, Math16_Wrap((int32_t)p.x + 4), p.y, 24, 32, 0, 1);
        const ENTITY_POINT player = g_Player->position[g_Stage->scroll.front];
        if (player.x > Math16_Wrap((int32_t)p.x - 16)
            && player.x < Math16_Wrap((int32_t)p.x + 16)
            && player.y > Math16_Wrap((int32_t)p.y - 16)
            && player.y < Math16_Wrap((int32_t)p.y + 16)) {
            Player_Hit();
        }
        Draw_SpriteRect(&patterns->entries[137 + (i & 3)], p.x, p.y);
    }
}

static void M_SphereCleanup(void)
{
    for (int i = 0; i < MIMA_SPHERE_COUNT; i++) {
        const ENTITY_POINT p = m_Mima.sphere_position[g_Stage->scroll.back][i];
        if (m_Mima.sphere_active[i] != 0 && p.y > -16 && p.y < 382) {
            Background_Mark(g_Stage->scroll.line, p.x, p.y, 32, 32);
            if (m_Mima.sphere_active[i] == 2) {
                m_Mima.sphere_active[i] = 0;
            }
        }
    }
}

static void M_BackgroundHalf(
    const uint8_t color, const uint8_t offset, const int step, const bool disc,
    const int start)
{
    const uint8_t radius = m_Mima.background_radius;
    Grcg_SetColor(color);
    for (int i = start; i < start + 128; i += step) {
        const uint8_t angle = i;
        const int16_t x = Math16_PolarX(224, radius, angle);
        const int16_t y = Math16_PolarY(200, radius, (uint8_t)(angle + offset));
        if (x > 24 && x < 416 && y > 8 && y < 384) {
            Draw_Dot(x, y, (radius >> 6) + 1);
        }
        if (disc && i == 256) {
            Grcg_SetColor(m_Mima.disc_color);
            Draw_CircleFill(224, 200, m_Mima.disc_radius);
            Grcg_SetColor(color);
        }
    }
    Grcg_Off();
}

static void M_Rank(const uint8_t rank)
{
    const bool easy = rank == 0;
    m_Mima.ring = BULLET_RING16;
    m_Mima.groups[0] = easy ? BULLET_AIMED : BULLET_FAN5_MEDIUM_AIMED;
    m_Mima.groups[1] = easy ? BULLET_FAN2_MEDIUM_AIMED : BULLET_RING32;
    m_Mima.ball_group = easy ? BULLET_RING2 : BULLET_RING4;
    m_Mima.angle_step = easy ? 8 : 6;
}

static void M_Sweep(BOSS_STATE *const boss, const uint8_t rank)
{
    if (boss->phase_frame < 10) {
        return;
    }
    if (boss->phase_frame == 10) {
        Bullets_SetSpawnLayers(0);
        boss->sprite = 131;
    }
    if (boss->phase_frame < 66) {
        M_Position(boss)->x = Math16_Wrap((int32_t)M_Position(boss)->x - (2));
        if (M_Position(boss)->x <= 32) {
            boss->phase_frame = 66;
        }
    } else if (boss->phase_frame < 178) {
        M_Position(boss)->x = Math16_Wrap((int32_t)M_Position(boss)->x + (2));
        if (M_Position(boss)->x >= 256) {
            boss->phase_frame = 178;
        }
    } else if (boss->phase_frame < 234) {
        M_Position(boss)->x = Math16_Wrap((int32_t)M_Position(boss)->x - (2));
        if (M_Position(boss)->x <= 144) {
            boss->phase_frame = 234;
        }
    } else {
        boss->phase_frame = 0;
        boss->sprite = 128;
        Bullets_SetSpawnLayers(1);
    }
    if ((boss->phase_frame % 32) == 0) {
        Bullets_AddPellets(
            g_Stage->scroll.back, m_Mima.pellet_origin.x,
            m_Mima.pellet_origin.y, (Random_RingU8And(0x0F) + 0x38),
            m_Mima.ring, 60);
        Bullets_AddPellets(
            g_Stage->scroll.back, m_Mima.pellet_origin.x,
            m_Mima.pellet_origin.y, 0x00, m_Mima.groups[m_Mima.form], 40);
    }
}

static void M_Beam(BOSS_STATE *const boss, const uint8_t rank)
{
    uint8_t angle;
    uint8_t angle_step;
    int16_t y;
    int i;
    int16_t x;

    if (boss->phase_frame < 20) {
        return;
    }
    if (boss->phase_frame < 70) {
        boss->sprite = 128;
        M_MoveToward(boss);
    } else if (boss->phase_frame == 70) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 131;
        m_Mima.beam_angle = 0x30;
        m_Mima.beam_spread = 0;
        m_Mima.beam_tone = 100;
    } else if (boss->phase_frame < 170) {
        Grcg_SetColor(13);
        angle_step = (0x40 - m_Mima.beam_spread);
        angle_step >>= 2;
        for (i = 0, angle = m_Mima.beam_spread; i < 9;
             angle += angle_step, i++) {
            x = Math16_PolarX(m_Mima.pellet_origin.x, 400, angle);
            y = Math16_PolarY(m_Mima.pellet_origin.y, 400, angle);
            Draw_Line(
                g_Clip, m_Mima.pellet_origin.x, m_Mima.pellet_origin.y, x, y);
        }
        if (boss->phase_frame >= 36) {
            m_Mima.beam_spread++;
        }
        Grcg_Off();
        if (boss->phase_frame & 1) {
            m_Mima.beam_tone++;
        }
        Palette_SetTone(m_Mima.beam_tone);
    } else if (boss->phase_frame == 170) {
        SoundEffects_Play(SOUND_EFFECT_MIMA_BEAM);
        boss->sprite = 134;
    } else if (boss->phase_frame <= 198) {
        M_Position(boss)->y = (272 - ((boss->phase_frame & 3) * 60));
        Grcg_SetColor(13);
        angle = Random_RingU8And(0x7F);
        x = (angle + M_Position(boss)->x + 8);
        Draw_Line(g_Clip, x, 16, x, (384 - 1));
        Grcg_Off();
        if (((M_Position(boss)->x + 16) < g_Player->current.x)
            && ((M_Position(boss)->x + 96) > g_Player->current.x)) {
            Player_Hit();
        }
        if (false) {
            m_Mima.beam_tone = 150;
        } else {
            m_Mima.beam_tone = 100;
        }
        Palette_SetTone(m_Mima.beam_tone);
    } else if (boss->phase_frame <= 262) {
        boss->sprite = 128;
        M_Position(boss)->y = Math16_Wrap((int32_t)M_Position(boss)->y - (4));
        if (M_Position(boss)->y <= 64) {
            boss->phase_frame = 263;
        }
        if ((rank != 0) && ((boss->phase_frame % 16) == 0)) {
            if (m_Mima.form == 0) {
                angle = BULLET_RING16;
            } else {
                angle = BULLET_RING32;
            }
            Bullets_AddPellets(
                g_Stage->scroll.back, m_Mima.shot_origin.x,
                m_Mima.shot_origin.y, 0x00, angle, 60);
        }
    } else {
        boss->phase_frame = 0;
    }
}

static void M_Orbit(
    BOSS_STATE *const boss, int16_t *velocity_x, int16_t *velocity_y)
{
    int i;
    int j;
    uint8_t angle;
    int16_t left;
    int16_t top;

    if (boss->phase_frame == 40) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 131;
        m_Mima.orbit_center.x = (M_Position(boss)->x + 80);
        m_Mima.orbit_center.y = (M_Position(boss)->y + 64);
        m_Mima.orbit_center.x = Math16_FromPixel(m_Mima.orbit_center.x);
        m_Mima.orbit_center.y = Math16_FromPixel(m_Mima.orbit_center.y);
        m_Mima.orbit_radius = 4;
        m_Mima.orbit_direction = -1;
        m_Mima.orbit_spinning = 0;
    } else if (boss->phase_frame < 80) {
        return;
    } else if (boss->phase_frame == 80) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
        boss->sprite = 134;
        for (i = 0, angle = m_Mima.orbit_angle; i < 4; i++, angle += 0x40) {
            m_Mima.sphere_position[0][i].x = m_Mima.sphere_position[1][i].x =
                Math16_PolarX(
                    (m_Mima.orbit_center.x >> 4), m_Mima.orbit_radius, angle);
            m_Mima.sphere_position[0][i].y = m_Mima.sphere_position[1][i].y =
                Math16_PolarY(
                    (m_Mima.orbit_center.y >> 4), m_Mima.orbit_radius, angle);
            m_Mima.sphere_active[i] = 1;
        }
    } else if (boss->phase_frame < 96) {
        m_Mima.orbit_radius = Math16_Wrap((int32_t)m_Mima.orbit_radius + (4));
        for (i = 0, angle = m_Mima.orbit_angle; i < 4; i++, angle += 0x40) {
            m_Mima.sphere_position[g_Stage->scroll.back][i].x = Math16_PolarX(
                (m_Mima.orbit_center.x >> 4), m_Mima.orbit_radius, angle);
            m_Mima.sphere_position[g_Stage->scroll.back][i].y = Math16_PolarY(
                (m_Mima.orbit_center.y >> 4), m_Mima.orbit_radius, angle);
        }
    } else {
        m_Mima.orbit_center.x =
            Math16_Wrap((int32_t)m_Mima.orbit_center.x + ((*velocity_x)));
        m_Mima.orbit_center.y =
            Math16_Wrap((int32_t)m_Mima.orbit_center.y + ((*velocity_y)));
        m_Mima.orbit_radius = Math16_Wrap(
            (int32_t)m_Mima.orbit_radius + (m_Mima.orbit_direction));
        if (m_Mima.orbit_burst_mode == 0) {
            if (m_Mima.form == 0) {
                goto radius_bounce;
            }
            if (m_Mima.orbit_radius == 0xC4) {
                boss->phase_frame = 0;
            }
            if (m_Mima.orbit_radius < 0x14) {
                m_Mima.orbit_spinning = 0;
                SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
                Bullets_SetSpawnLayers(0);
                for (i = 0; i < 4; i++) {
                    left =
                        (m_Mima.sphere_position[g_Stage->scroll.back][i].x
                         + 12);
                    top =
                        (m_Mima.sphere_position[g_Stage->scroll.back][i].y
                         + 12);
                    for (j = 0,
                        angle = ((i * 0x40) + m_Mima.orbit_angle - 0x40);
                         j < 16; j++, angle += 8) {
                        Bullets_AddPellets(
                            g_Stage->scroll.back, left, top, angle,
                            BULLET_SINGLE, 35);
                    }
                    for (j = 0,
                        angle = ((i * 0x40) + m_Mima.orbit_angle - 0x3C);
                         j < 16; j++, angle += 8) {
                        Bullets_AddPellets(
                            g_Stage->scroll.back, left, top, angle,
                            BULLET_SINGLE, (35 - j));
                    }
                    m_Mima.sphere_active[i] = 2;
                }
                Bullets_SetSpawnLayers(1);
                m_Mima.orbit_radius = 0x140;
                boss->sprite = 128;
            }
        } else {
            if (m_Mima.orbit_spinning != 0) {
                if (m_Mima.form != 0) {
                    m_Mima.orbit_angle += -3;
                } else {
                    m_Mima.orbit_angle += -5;
                }
            }
            if (m_Mima.orbit_radius >= 400) {
                goto remove;
            }
            if (m_Mima.orbit_radius < 0x14) {
                (*velocity_x) = 0;
                (*velocity_y) = 0;
                m_Mima.orbit_spinning = 1;
                SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
                angle = (m_Mima.orbit_angle + 8);
                for (i = 0; i < 4; i++, angle += 0x40) {
                    left =
                        (m_Mima.sphere_position[g_Stage->scroll.back][i].x
                         + 12);
                    top =
                        (m_Mima.sphere_position[g_Stage->scroll.back][i].y
                         + 12);
                    for (j = 0; j < 48; j += 12) {
                        Bullets_AddPellets(
                            g_Stage->scroll.back, left, top, (angle + j),
                            BULLET_SINGLE, (j + 35));
                    }
                }
                boss->sprite = 128;
                m_Mima.orbit_direction = 5;
            }
        }
        if (false) {
        radius_bounce:
            if (m_Mima.orbit_radius < 32) {
                m_Mima.orbit_direction = 2;
            } else if (m_Mima.orbit_radius > 128) {
                m_Mima.orbit_direction = -2;
            }
        }
        left = (m_Mima.orbit_center.x >> 4);
        top = (m_Mima.orbit_center.y >> 4);
        if (((left + m_Mima.orbit_radius) < 0)
            || ((left - m_Mima.orbit_radius) > 0x1A0)
            || ((top + m_Mima.orbit_radius) < -16)
            || ((top - m_Mima.orbit_radius) > 0x182)
            || (boss->phase_frame > 500)) {
        remove:
            for (i = 0; i < 4; i++) {
                m_Mima.sphere_active[i] = 2;
            }
            boss->phase_frame = 0;
            boss->sprite = 128;
        }
        for (i = 0, angle = m_Mima.orbit_angle; i < 4; i++, angle += 0x40) {
            m_Mima.sphere_position[g_Stage->scroll.back][i].x = Math16_PolarX(
                (m_Mima.orbit_center.x >> 4), m_Mima.orbit_radius, angle);
            m_Mima.sphere_position[g_Stage->scroll.back][i].y = Math16_PolarY(
                (m_Mima.orbit_center.y >> 4), m_Mima.orbit_radius, angle);
        }
    }
    m_Mima.orbit_angle += 5;
}

static void M_AimedOrbit(BOSS_STATE *const boss, const uint8_t rank)
{
    int16_t left;
    int16_t top;

    if (boss->phase_frame < 40) {
        return;
    }
    if (boss->phase_frame == 40) {
        m_Mima.orbit_burst_mode = 0;
        left = (M_Position(boss)->x + 80);
        top = (M_Position(boss)->y + 64);
        m_Mima.aimed_velocity = M_Aim(left, top, 48);
    }
    M_Orbit(boss, &m_Mima.aimed_velocity.x, &m_Mima.aimed_velocity.y);
}

static void M_Spiral(BOSS_STATE *const boss, const uint8_t rank)
{
    if (boss->phase_frame < 10) {
        return;
    }
    if (boss->phase_frame < 70) {
        boss->sprite = 128;
        M_MoveToward(boss);
    } else if (boss->phase_frame == 70) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 131;
        m_Mima.spiral_speed = 20;
    } else if (boss->phase_frame < 100) {
        return;
    } else if (boss->phase_frame == 100) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
        boss->sprite = 134;
        m_Mima.spiral_angle = Random_RingU8();
    } else if (boss->phase_frame <= 160) {
        m_Mima.spiral_angle--;
    } else if (boss->phase_frame <= 240) {
        m_Mima.spiral_angle++;
        m_Mima.spiral_speed++;
    } else if (m_Mima.form != 0) {
        if (boss->phase_frame <= 320) {
            m_Mima.spiral_angle++;
            m_Mima.spiral_speed--;
        } else if (boss->phase_frame <= 400) {
            m_Mima.spiral_angle--;
            m_Mima.spiral_speed++;
        } else {
            boss->phase_frame = 0;
            boss->sprite = 128;
        }
    } else {
        boss->phase_frame = 0;
        boss->sprite = 128;
    }
    if ((boss->phase_frame > 100) && ((boss->phase_frame % (8 - rank)) == 0)) {
        Bullets_AddPellets(
            g_Stage->scroll.back, m_Mima.lower_origin.x, m_Mima.lower_origin.y,
            m_Mima.spiral_angle, m_Mima.ball_group, m_Mima.spiral_speed);
        SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
    }
}

static void M_Fan(BOSS_STATE *const boss, const uint8_t rank)
{
    if (boss->phase_frame < 10) {
        return;
    }
    if (boss->phase_frame < 70) {
        boss->sprite = 128;
        M_MoveToward(boss);
    } else if (boss->phase_frame == 70) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 131;
        m_Mima.fan_angle = -0x28;
    } else if (boss->phase_frame < 100) {
        return;
    } else if (boss->phase_frame == 100) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
        boss->sprite = 134;
    } else if (boss->phase_frame <= 132) {
        Bullets_AddPellets(
            g_Stage->scroll.back, m_Mima.lower_origin.x, m_Mima.lower_origin.y,
            m_Mima.fan_angle, BULLET_MIRROR, 90);
        m_Mima.fan_angle += 3;
    } else if (boss->phase_frame <= 150) {
        Bullets_AddPellets(
            g_Stage->scroll.back, m_Mima.lower_origin.x, m_Mima.lower_origin.y,
            m_Mima.fan_angle, BULLET_MIRROR, 90);
        m_Mima.fan_angle += -3;
    } else if (boss->phase_frame <= 226) {
        if ((boss->phase_frame % 8) == 1) {
            if (m_Mima.form == 0) {
                Bullets_AddPellets(
                    g_Stage->scroll.back, m_Mima.lower_origin.x,
                    m_Mima.lower_origin.y, 0x40, BULLET_FAN3, 90);
            } else {
                Bullets_AddPellets(
                    g_Stage->scroll.back, m_Mima.lower_origin.x,
                    m_Mima.lower_origin.y, (Random_RingU8And(0x3F) + 0x20),
                    BULLET_SINGLE, 80);
                Bullets_AddPellets(
                    g_Stage->scroll.back, m_Mima.lower_origin.x,
                    m_Mima.lower_origin.y, (Random_RingU8And(0x3F) + 0x20),
                    BULLET_SINGLE, 80);
            }
        }
    } else {
        boss->phase_frame = 0;
        boss->sprite = 128;
    }
}

static void M_TwistOrbit(BOSS_STATE *const boss, const uint8_t rank)
{
    int16_t left;
    int16_t top;

    if (boss->phase_frame < 40) {
        return;
    }
    if (boss->phase_frame == 40) {
        m_Mima.orbit_burst_mode = 1;
        left = (M_Position(boss)->x + 80);
        top = (M_Position(boss)->y + 64);
        m_Mima.twist_velocity = M_Aim(left, top, 52);
    }
    M_Orbit(boss, &m_Mima.twist_velocity.x, &m_Mima.twist_velocity.y);
}

static void M_Drop(BOSS_STATE *const boss, const uint8_t rank)
{
    if (boss->phase_frame < 10) {
        return;
    }
    if (boss->phase_frame == 10) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 131;
    } else if (boss->phase_frame < 40) {
        return;
    } else if (boss->phase_frame == 40) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
        boss->sprite = 134;
        if (m_Mima.phase == MIMA_CHANGE_FOUR) {
            Items_Add(m_Mima.lower_origin.x, m_Mima.lower_origin.y, ITEM_BOMB);
        } else {
            Items_Add(
                m_Mima.lower_origin.x, m_Mima.lower_origin.y, ITEM_BIG_POWER);
        }
    } else if (boss->phase_frame == 60) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
        Items_Add(m_Mima.lower_origin.x, m_Mima.lower_origin.y, ITEM_BIG_POWER);
    } else if (boss->phase_frame == 80) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
        boss->sprite = 128;
        if (m_Mima.phase == MIMA_CHANGE_TWO) {
            Items_Add(m_Mima.lower_origin.x, m_Mima.lower_origin.y, ITEM_LIFE);
        } else {
            Items_Add(
                m_Mima.lower_origin.x, m_Mima.lower_origin.y, ITEM_BIG_POWER);
        }
    }
}

static void M_Stars(BOSS_STATE *const boss, const uint8_t rank)
{
    if (boss->phase_frame < 10) {
        return;
    }
    if (boss->phase_frame < 70) {
        boss->sprite = 128;
        M_MoveToward(boss);
    } else if (boss->phase_frame == 70) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 131;
    } else if (boss->phase_frame < 100) {
        return;
    } else if (boss->phase_frame == 100) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
        boss->sprite = 134;
        m_Mima.star_angle = Math16_Angle(
            (g_Player->current.y - m_Mima.lower_origin.y),
            (g_Player->current.x - m_Mima.lower_origin.x));
        m_Mima.star_direction = (g_Player->current.x <= (208)) ? -1 : 1;
        Bullets_SetTurnLimit(m_Mima.form);
    } else if (boss->phase_frame <= 300) {
        if ((boss->phase_frame % 8) == 0) {
            SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
            if (rank != 0) {
                M_Star(+0x0A);
                M_Star(-0x0A);
                M_Star(+0x1E);
                M_Star(-0x1E);
            } else {
                M_Star(+0x0F);
                M_Star(-0x0F);
            }
        }
        if ((boss->phase_frame >= 150) && (boss->phase_frame & 1)) {
            if (boss->phase_frame < 210) {
                m_Mima.star_angle += m_Mima.star_direction;
            } else {
                m_Mima.star_angle -= m_Mima.star_direction;
            }
        }
    } else {
        boss->phase_frame = 0;
        boss->sprite = 128;
    }
}

static void M_Cross(BOSS_STATE *const boss, const uint8_t rank)
{
    if (boss->phase_frame < 30) {
        return;
    }
    if (boss->phase_frame < 70) {
        boss->sprite = 128;
        M_MoveToward(boss);
    } else if (boss->phase_frame == 70) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 131;
    } else if (boss->phase_frame < 120) {
        return;
    } else if (boss->phase_frame == 120) {
        SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        boss->sprite = 134;
        m_Mima.cross_angle = 0x04;
        Bullets_AddPellets(
            g_Stage->scroll.back, m_Mima.lower_origin.x, m_Mima.lower_origin.y,
            (m_Mima.cross_angle + 8), BULLET_FAN3, 80);
        Bullets_AddPellets(
            g_Stage->scroll.back, m_Mima.lower_origin.x, m_Mima.lower_origin.y,
            (0x78 - m_Mima.cross_angle), BULLET_FAN3, 80);
        m_Mima.cross_angle += 10;
    } else if (boss->phase_frame <= 160) {
        if ((boss->phase_frame % 10) == 0) {
            SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
            Bullets_AddPellets(
                g_Stage->scroll.back, m_Mima.lower_origin.x,
                m_Mima.lower_origin.y, (m_Mima.cross_angle + 8), BULLET_FAN3,
                80);
            Bullets_AddPellets(
                g_Stage->scroll.back, m_Mima.lower_origin.x,
                m_Mima.lower_origin.y, (0x78 - m_Mima.cross_angle), BULLET_FAN3,
                80);
            m_Mima.cross_angle += 10;
        }
    } else {
        boss->phase_frame = 0;
        boss->sprite = 128;
    }
}

static void M_Expand(BOSS_STATE *const boss, const uint8_t rank)
{
    int i;
    uint8_t pellet_angle;
    uint8_t angle;

    if (boss->phase_frame < 40) {
        return;
    }
    if (boss->phase_frame == 40) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 131;
        m_Mima.expand_center.x = (M_Position(boss)->x + 80);
        m_Mima.expand_center.y = (M_Position(boss)->y + 80);
        m_Mima.expand_radius = 4;
        m_Mima.expand_angle = Random_RingU8();
    } else if (boss->phase_frame < 80) {
        return;
    } else if (boss->phase_frame == 80) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
        boss->sprite = 134;
        for (i = 0, angle = m_Mima.expand_angle; i < 4; i++, angle += 0x40) {
            m_Mima.sphere_position[0][i].x = m_Mima.sphere_position[1][i].x =
                Math16_PolarX(
                    (m_Mima.expand_center.x >> 4), m_Mima.expand_radius, angle);
            m_Mima.sphere_position[0][i].y = m_Mima.sphere_position[1][i].y =
                Math16_PolarY(
                    (m_Mima.expand_center.y >> 4), m_Mima.expand_radius, angle);
            m_Mima.sphere_active[i] = 1;
        }
    } else {
        m_Mima.expand_radius = Math16_Wrap((int32_t)m_Mima.expand_radius + (2));
        for (i = 0, angle = m_Mima.expand_angle; i < 4; i++, angle += 0x40) {
            m_Mima.sphere_position[g_Stage->scroll.back][i].x = Math16_PolarX(
                m_Mima.expand_center.x, m_Mima.expand_radius, angle);
            m_Mima.sphere_position[g_Stage->scroll.back][i].y = Math16_PolarY(
                m_Mima.expand_center.y, m_Mima.expand_radius, angle);
            if (((180 - (rank * 16)) < m_Mima.expand_radius)
                && ((boss->phase_frame & 3) == 0)) {
                SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
                if ((m_Mima.sphere_position[g_Stage->scroll.back][i].x > 0)
                    && (m_Mima.sphere_position[g_Stage->scroll.back][i].x
                        < 0x1A0)
                    && (m_Mima.sphere_position[g_Stage->scroll.back][i].y > 0)
                    && (m_Mima.sphere_position[g_Stage->scroll.back][i].y
                        < 0x190)) {
                    pellet_angle = (m_Mima.form == 0)
                        ? ((i * 0x40) + m_Mima.expand_angle + 0x40)
                        : ((i * 0x40) + m_Mima.expand_angle + 0x60);
                    Bullets_AddPellets(
                        g_Stage->scroll.back,
                        m_Mima.sphere_position[g_Stage->scroll.back][i].x,
                        m_Mima.sphere_position[g_Stage->scroll.back][i].y,
                        pellet_angle, BULLET_SINGLE, 128);
                }
            }
        }
        if (m_Mima.expand_radius > 0x1CC) {
            for (i = 0; i < 4; i++) {
                m_Mima.sphere_active[i] = 0;
            }
            boss->phase_frame = 0;
            boss->sprite = 128;
        }
    }
    m_Mima.expand_angle += -3;
}

static void M_Balls(BOSS_STATE *const boss, const uint8_t rank)
{
    if (boss->phase_frame < 50) {
        return;
    }
    if (boss->phase_frame == 50) {
        m_Mima.ball_angle = 0;
    } else if (boss->phase_frame < 150) {
        if (boss->phase_frame & 1) {
            SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
            Bullets_AddSprites(
                g_Stage->scroll.back, m_Mima.ball_origin.x,
                m_Mima.ball_origin.y, m_Mima.ball_angle, BULLET_MIRROR, 86, 50);
            m_Mima.ball_angle += m_Mima.angle_step;
        }
    } else {
        boss->phase_frame = 0;
    }
}

static void M_RandomPairs(BOSS_STATE *const boss, const uint8_t rank)
{
    if (boss->phase_frame < 50) {
        return;
    }
    if (boss->phase_frame == 50) {
        m_Mima.cue_angle = Math16_Angle(
            ((g_Player->current.y + 12) - m_Mima.ball_origin.y),
            ((g_Player->current.x + 12) - m_Mima.ball_origin.x));
        m_Mima.cue_radius = 30;
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
    } else if (boss->phase_frame <= 80) {
        if (boss->phase_frame & 1) {
            M_Circles(0);
            m_Mima.cue_radius += -2;
            if (boss->phase_frame != 80) {
                M_Circles(13);
                Grcg_Off();
            }
        }
    } else if (boss->phase_frame < 200) {
        if ((boss->phase_frame & 7) == 0) {
            M_RandomPair();
        }
        if ((rank > 0) && ((boss->phase_frame & 7) == 4)) {
            M_RandomPair();
        }
        if ((boss->phase_frame & 7) == 0) {
            SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        }
    } else {
        boss->phase_frame = 0;
    }
}

static void M_AimedBalls(BOSS_STATE *const boss, const uint8_t rank)
{
    if (boss->phase_frame < 50) {
        return;
    }
    if (boss->phase_frame < 190) {
        if ((boss->phase_frame & 0x1F) == 0) {
            Bullets_AddSprites(
                g_Stage->scroll.back, m_Mima.ball_origin.x,
                m_Mima.ball_origin.y, 0, BULLET_FAN5_MEDIUM_AIMED, 86, 90);
        } else if ((boss->phase_frame & 0x1F) == 16) {
            Bullets_AddSprites(
                g_Stage->scroll.back, m_Mima.ball_origin.x,
                m_Mima.ball_origin.y, 0, BULLET_FAN4_MEDIUM_AIMED, 86, 90);
        }
    } else {
        boss->phase_frame = 0;
    }
}

static void M_Advance(BOSS_STATE *const boss)
{
    int pattern;

    if (boss->phase_frame == 0) {
        if ((boss->damage >= m_Mima.damage_threshold)
            || (m_Mima.pattern_cycles > m_Mima.max_patterns)) {
            m_Mima.phase = Math16_Wrap((int32_t)m_Mima.phase + 1);
            m_Mima.damage_enable = 0;
            m_Mima.pattern_cycles = 0;
        } else {
            m_Mima.pattern_cycles =
                Math16_Wrap((int32_t)m_Mima.pattern_cycles + 1);
            if (m_Mima.pattern_cycles >= m_Mima.min_patterns) {
                m_Mima.damage_enable = 1;
            }
            if (m_Mima.form == 0) {
                pattern = (m_Mima.pattern + 1);
                if (pattern >= m_Mima.pattern_count) {
                    pattern = 0;
                }
            } else {
                pattern = (m_Mima.pattern + 1);
                if (pattern > 7) {
                    pattern = 0;
                }
            }
            m_Mima.pattern = pattern;
            if (M_Position(boss)->y < 48) {
                m_Mima.move_y = 1;
            } else if (M_Position(boss)->y > 64) {
                m_Mima.move_y = -1;
            } else {
                m_Mima.move_y = (1 - (Random_RingU8() % 3));
            }
        }
    }
    if (boss->phase_frame < 10) {
        M_Position(boss)->y =
            Math16_Wrap((int32_t)M_Position(boss)->y + (m_Mima.move_y));
    }
}

static void M_Control(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns,
    const uint8_t rank, const bool reduce_effects)
{
    uint8_t radius;
    int i;

    boss->phase_frame = Math16_Wrap((int32_t)boss->phase_frame + 1);
    Shots_SetTarget(
        (ENTITY_POINT) { M_Position(boss)->x + 72, M_Position(boss)->y + 56 });
    m_Mima.shot_origin.x = (M_Position(boss)->x + 32);
    m_Mima.pellet_origin.x = (M_Position(boss)->x + 40);
    m_Mima.lower_origin.x = (M_Position(boss)->x + 64);
    m_Mima.ball_origin.x = (M_Position(boss)->x + 64);
    m_Mima.shot_origin.y = (M_Position(boss)->y + 96);
    m_Mima.pellet_origin.y = (M_Position(boss)->y + 16);
    m_Mima.lower_origin.y = (M_Position(boss)->y + 114);
    m_Mima.ball_origin.y = (M_Position(boss)->y + 44);

    if ((g_Stage->world.frame & 1) == 0) {
        if (!reduce_effects) {
            m_Mima.effect_radius += 8;
            Grcg_SetColor(3);
            for (i = 0, radius = m_Mima.effect_radius; i < 2;
                 i++, radius += 0x80) {
                M_Ring(radius);
            }
            Grcg_Off();
        }
        if (m_Mima.phase & 1) {
            m_Mima.disc_pulse++;
            if (m_Mima.disc_pulse >= 0x40) {
                m_Mima.disc_pulse = 0;
            }
            m_Mima.disc_radius =
                (m_Mima.disc_base_radius - (m_Mima.disc_pulse / 4));
        }
    }

    if (m_Mima.phase == MIMA_APPEAR) {
        m_Mima.background_radius = (boss->phase_frame / 2);
        m_Mima.disc_radius = (boss->phase_frame / 3);
        M_Palette((boss->phase_frame >> 1), 0, (boss->phase_frame >> 1));
        if (boss->phase_frame > 100) {
            m_Mima.disc_base_radius = m_Mima.disc_radius;
            m_Mima.disc_pulse = 0;
            m_Mima.phase = MIMA_ATTACK_ONE;
            boss->phase_frame = 0;
            m_Mima.pattern = (Random_RingU8() % 3);
            m_Mima.pattern_cycles = 0;
            m_Mima.damage_threshold = 600;
            m_Mima.min_patterns = 2;
            m_Mima.max_patterns = 10;
            m_Mima.pattern_count = 3;
            Boss_ResetDamage(&boss->damage, m_Mima.damage_threshold);
        }
    } else if (m_Mima.phase == MIMA_ATTACK_ONE) {
        static const MIMA_ATTACK attacks[] = {
            M_Sweep,
            M_Beam,
            M_AimedOrbit,
        };
        if (m_Mima.pattern >= 0 && m_Mima.pattern < (int)ARRAY_SIZE(attacks)) {
            attacks[m_Mima.pattern](boss, rank);
        }
        M_Advance(boss);
    } else if (m_Mima.phase == MIMA_CHANGE_TWO) {
        m_Mima.background_radius = ((boss->phase_frame / 4) + 50);
        m_Mima.disc_radius = ((boss->phase_frame / 5) + 30);
        M_Drop(boss, rank);
        M_Palette(
            (51 - (boss->phase_frame >> 1)), (boss->phase_frame >> 1), 50);
        if (boss->phase_frame > 100) {
            m_Mima.phase = MIMA_ATTACK_TWO;
            boss->phase_frame = 0;
            m_Mima.pattern = (Random_RingU8() % 3);
            m_Mima.pattern_cycles = 0;
            m_Mima.disc_base_radius = m_Mima.disc_radius;
            m_Mima.damage_threshold = 700;
            goto phase_init;
        }
    } else if (m_Mima.phase == MIMA_ATTACK_TWO) {
        static const MIMA_ATTACK attacks[] = {
            M_Spiral,
            M_Fan,
            M_TwistOrbit,
        };
        if (m_Mima.pattern >= 0 && m_Mima.pattern < (int)ARRAY_SIZE(attacks)) {
            attacks[m_Mima.pattern](boss, rank);
        }
        M_Advance(boss);
    } else if (m_Mima.phase == MIMA_CHANGE_THREE) {
        m_Mima.background_radius = ((boss->phase_frame / 4) + 75);
        m_Mima.disc_radius = ((boss->phase_frame / 5) + 50);
        M_Drop(boss, rank);
        M_Palette(
            (boss->phase_frame >> 1), (51 - (boss->phase_frame >> 1)),
            (51 - (boss->phase_frame >> 1)));
        if (boss->phase_frame > 100) {
            m_Mima.phase = MIMA_ATTACK_THREE;
            boss->phase_frame = 0;
            m_Mima.pattern = (Random_RingU8() % 3);
            m_Mima.disc_base_radius = m_Mima.disc_radius;
            m_Mima.pattern_cycles = 0;
            m_Mima.damage_threshold = 800;
        phase_init:
            m_Mima.min_patterns = 2;
            m_Mima.max_patterns = 12;
            m_Mima.pattern_count = 3;
            Boss_ResetDamage(&boss->damage, m_Mima.damage_threshold);
        }
    } else if (m_Mima.phase == MIMA_ATTACK_THREE) {
        static const MIMA_ATTACK attacks[] = {
            M_Stars,
            M_Cross,
            M_Expand,
        };
        if (m_Mima.pattern >= 0 && m_Mima.pattern < (int)ARRAY_SIZE(attacks)) {
            attacks[m_Mima.pattern](boss, rank);
        }
        M_Advance(boss);
    } else if (m_Mima.phase == MIMA_CHANGE_FOUR) {
        m_Mima.background_radius = ((boss->phase_frame / 4) + 100);
        m_Mima.disc_radius = ((boss->phase_frame / 5) + 70);
        M_Drop(boss, rank);
        M_Palette((51 - (boss->phase_frame >> 1)), 0, 0);
        if (boss->phase_frame > 100) {
            m_Mima.disc_base_radius = m_Mima.disc_radius;
            m_Mima.phase = MIMA_ATTACK_FOUR;
            boss->phase_frame = 0;
            m_Mima.pattern = Random_RingU8And(7);
            m_Mima.pattern_cycles = 0;
            m_Mima.damage_threshold = 1500;
            m_Mima.min_patterns = 3;
            m_Mima.max_patterns = 200;
            m_Mima.pattern_count = 9;
            m_Mima.form = 1;
            Boss_ResetDamage(&boss->damage, m_Mima.damage_threshold);
            m_Mima.trail_color = 2;
        }
    } else if (m_Mima.phase == MIMA_ATTACK_FOUR) {
        static const MIMA_ATTACK attacks[] = {
            M_Stars,      M_AimedOrbit, M_Spiral, M_Beam,   M_Fan,
            M_TwistOrbit, M_Sweep,      M_Cross,  M_Expand,
        };
        if (m_Mima.pattern >= 0 && m_Mima.pattern < (int)ARRAY_SIZE(attacks)) {
            attacks[m_Mima.pattern](boss, rank);
        }
        M_Advance(boss);
    } else if (m_Mima.phase == MIMA_ATTACK_FINAL) {
        static const MIMA_ATTACK attacks[] = {
            M_Balls,
            M_RandomPairs,
            M_AimedBalls,
        };
        if (m_Mima.pattern >= 0 && m_Mima.pattern < (int)ARRAY_SIZE(attacks)) {
            attacks[m_Mima.pattern](boss, rank);
        }
        M_Advance(boss);
        if (boss->phase_frame == 1) {
            M_Position(boss)->y = 64;
        }
        if (boss->phase_frame < 30) {
            M_Position(boss)->x = Math16_Wrap(
                (int32_t)M_Position(boss)->x
                + (((m_Mima.ball_origin.x < g_Player->current.x) ? 1 : -1)));
        }
    }
    M_BodyHit(boss);
    Mima_DrawBody(boss, patterns);
    M_Spheres(patterns);
}

static void M_DrawBody(
    const BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    const ENTITY_POINT position = boss->position[g_Stage->scroll.back];
    for (int part = 0; part < 3; part++) {
        const int id = boss->sprite + part;
        ASSERT(id >= 0 && id < PATTERN_LIMIT);
        const PATTERN *const pattern = &patterns->entries[id];
        ASSERT(pattern->data != nullptr);
        Draw_SpriteRect(
            pattern, Math16_Wrap((int32_t)position.x + part * 48), position.y);
    }
}

static void Mima_Init(void)
{
    memset(&m_Mima, 0, sizeof(m_Mima));
}

void Mima_SelectPatterns(
    PATTERN_BANK *const target, const MIMA_ASSETS *const assets,
    const MIMA_BANK bank)
{
    ASSERT(bank >= MIMA_BANK_SUMMON && bank < MIMA_BANK_COUNT);
    if (assets == nullptr) {
        return;
    }
    const PATTERN_BANK *const patterns = &assets->patterns[bank];
    Boss_SelectPatterns(
        target, patterns,
        assets->has_palette[bank] ? assets->palettes[bank] : nullptr);
}

void Mima_Place(BOSS_STATE *const boss)
{
    boss->position[0] = boss->position[1] = (ENTITY_POINT) { 144, 64 };
    boss->sprite = 128;
}

void Mima_DrawBody(BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    if (boss->hit_flash != 0) {
        m_Mima.flash_cycle++;
        if ((m_Mima.flash_cycle & 3) == 0) {
            boss->hit_flash = 0;
            BossScene_DrawFlash(
                patterns, boss->sprite, *M_Position(boss), 3, 128);
            return;
        }
    }
    M_DrawBody(boss, patterns);
}

static void Mima_BeginScene(BOSS_STATE *const boss, const uint8_t rank)
{
    Shots_FreeAll();
    Boss_BeginStage(boss);
    m_Mima.damage_enable = 0;
    m_Mima.phase = MIMA_APPEAR;
    m_Mima.pattern_cycles = 0;
    m_Mima.background_radius = 0;
    m_Mima.disc_radius = 0;
    m_Mima.ring_color = 13;
    m_Mima.disc_color = 3;
    m_Mima.trail_color = 12;
    m_Mima.pattern = 0;
    m_Mima.form = 0;
    m_Mima.transition_started = false;
    m_Mima.ended = false;
    for (int i = 0; i < MIMA_SPHERE_COUNT; i++) {
        m_Mima.sphere_active[i] = 0;
    }
    M_Rank(rank);
    Background_SetMode(TILE_DISABLED);
    Video_AccessPage(g_Stage->scroll.front);
    Video_Clear();
    Video_AccessPage(g_Stage->scroll.back);
    Video_Clear();
    Grcg_SetColor(11);
    Clip_SetSidePanel();
    Video_AccessPage(g_Stage->scroll.front);
    Draw_BoxFill(g_Clip, 416, 0, 639, 399);
    Video_AccessPage(g_Stage->scroll.back);
    Draw_BoxFill(g_Clip, 416, 0, 639, 399);
    Grcg_Off();
    Clip_SetPlayfield();
}

static void Mima_Invalidate(BOSS_STATE *const boss, const bool reduce_effects)
{
    M_SphereCleanup();
    boss->position[g_Stage->scroll.back] =
        boss->position[g_Stage->scroll.front];
    const CLIP full = { 0, 0, 639, 399 };
    Grcg_SetColor(0);
    Draw_BoxFill(&full, 32, 16, 415, 383);
    const int step = reduce_effects ? 4 : 1;
    M_BackgroundHalf(
        m_Mima.trail_color, m_Mima.background_angle - 16, step, false, 128);
    M_BackgroundHalf(
        m_Mima.trail_color, m_Mima.background_angle - 8, step, false, 128);
    M_BackgroundHalf(
        m_Mima.ring_color, m_Mima.background_angle, step, false, 128);
    M_BackgroundHalf(
        m_Mima.trail_color, m_Mima.background_angle - 16, step, true, 256);
    M_BackgroundHalf(
        m_Mima.trail_color, m_Mima.background_angle - 8, step, false, 256);
    M_BackgroundHalf(
        m_Mima.ring_color, m_Mima.background_angle++, step, false, 256);
    Grcg_Off();
}

MIMA_TRANSITION Mima_BeginTransition(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    if (m_Mima.ended) {
        return MIMA_TRANSITION_ENDED;
    }
    if (m_Mima.transition_started) {
        return MIMA_TRANSITION_WAIT;
    }
    m_Mima.transition_started = true;
    if (m_Mima.phase == MIMA_FORM) {
        Shots_FreeAll();
        Bullets_RemoveAll(StageScroll_Pages());
    }
    M_DrawBody(boss, patterns);
    return m_Mima.phase == MIMA_FORM ? MIMA_TRANSITION_FORM
                                     : MIMA_TRANSITION_POST;
}

BOSS_RESULT Mima_EndTransition(BOSS_STATE *const boss, const bool continued)
{
    if (m_Mima.ended) {
        return BOSS_FINISHED;
    }
    if (m_Mima.phase == MIMA_POST || continued) {
        Score_AddBonus(m_Mima.phase == MIMA_FORM ? 50000 : 100000);
        m_Mima.ended = true;
        return BOSS_FINISHED;
    }
    Video_AccessPage(g_Stage->scroll.front);
    Grcg_SetColor(0);
    Draw_BoxFill(
        g_Clip, g_Clip->left, g_Clip->top, g_Clip->right, g_Clip->bottom);
    Video_AccessPage(g_Stage->scroll.back);
    Draw_BoxFill(
        g_Clip, g_Clip->left, g_Clip->top, g_Clip->right, g_Clip->bottom);
    Grcg_Off();
    m_Mima.form = 0;
    m_Mima.phase = MIMA_ATTACK_FINAL;
    boss->sprite = 128;
    m_Mima.pattern_cycles = 0;
    boss->phase_frame = 0;
    m_Mima.pattern = Random_RingU8() % 3;
    Boss_ResetDamage(&boss->damage, 1100);
    m_Mima.damage_threshold = 1100;
    m_Mima.min_patterns = 2;
    m_Mima.max_patterns = 200;
    m_Mima.pattern_count = 3;
    m_Mima.disc_color = 1;
    m_Mima.trail_color = 2;
    m_Mima.background_radius = 200;
    m_Mima.disc_radius = 150;
    m_Mima.disc_base_radius = m_Mima.disc_radius;
    m_Mima.transition_started = false;
    return BOSS_RUNNING;
}

static BOSS_RESULT Mima_Update(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns,
    const uint8_t rank, const bool reduce_effects)
{
    if (m_Mima.phase != MIMA_FORM && m_Mima.phase != MIMA_POST) {
        M_Control(boss, patterns, rank, reduce_effects);
    }
    if (m_Mima.phase == MIMA_FORM || m_Mima.phase == MIMA_POST) {
        return BOSS_WAIT;
    }
    return BOSS_RUNNING;
}

static void M_Begin(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const TH02_SESSION *const session)
{
    if (assets->stages[session->stage].boxes == nullptr) {
        Mima_SelectPatterns(
            patterns,
            assets->mima.patterns[MIMA_BANK_FIRST].next != 0 ? &assets->mima
                                                             : nullptr,
            MIMA_BANK_FIRST);
        Mima_Place(p_BossState);
    }
    Mima_BeginScene(p_BossState, (uint8_t)session->rank);
}

static void M_Invalidate(const bool reduce_effects)
{
    Mima_Invalidate(p_BossState, reduce_effects);
}

static BOSS_RESULT M_Update(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, TH02_SESSION *const session,
    const TH02_INPUT *const input)
{
    const BOSS_RESULT result = Mima_Update(
        p_BossState, patterns, (uint8_t)session->rank, session->reduce_effects);
    if (result == BOSS_WAIT) {
        const BOSS_RESULT transition = g_BossMima.talk.transition(
            p_BossState, patterns, assets, music, session, input);
        if (transition == BOSS_FINISHED) {
            session->stage = 127;
            return BOSS_WAIT;
        }
        return transition;
    }
    return result;
}

const BOSS_STRATEGY g_BossMima = {
    .init = Mima_Init,
    .begin = M_Begin,
    .invalidate = M_Invalidate,
    .update = M_Update,
    .post_dialogue = false,
    .talk = { .begin = Mima_TalkBegin,
              .update = Mima_TalkUpdate,
              .transition = Mima_Transition },
};
