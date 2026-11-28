#include "th02/background.h"
#include "th02/boss/private.h"
#include "th02/boss/sigma.h"
#include "th02/boss/sigma/assets.h"
#include "th02/boss/sigma/private.h"
#include "th02/boss/strategy.h"
#include "th02/bullets.h"
#include "th02/clip.h"
#include "th02/draw.h"
#include "th02/palette.h"
#include "th02/pillars.h"
#include "th02/player.h"
#include "th02/random.h"
#include "th02/score.h"
#include "th02/shots.h"
#include "th02/sound_effects.h"
#include "th02/stage.h"

#include "core/base/assert.h"
#include "core/base/result.h"
#include "pc98/zun/math16.h"

#include <string.h>

typedef void (*SIGMA_ATTACK_UPDATE)(BOSS_STATE *boss);

static SIGMA m_Sigma;
SIGMA *const p_Sigma = &m_Sigma;

static void M_Add(const int16_t x, int16_t y, const uint8_t group)
{
    if (y < 0) {
        y = Math16_Wrap((int32_t)y + 400);
    } else if (y >= 400) {
        y -= 400;
    }
    for (int i = 0; i < SIGMA_MIDBOSS_TILE_COUNT; i++) {
        SIGMA_MIDBOSS_TILE *const tile = &m_Sigma.tiles[i];
        if (tile->flag == 0) {
            tile->flag = 1;
            tile->frame = 0;
            tile->position = (ENTITY_POINT) { x, y };
            tile->group = group;
            tile->angle = 0;
            tile->speed = 100;
            return;
        }
    }
}

static void M_Cross(const int16_t x, const int16_t y, const uint8_t group)
{
    const uint8_t spread = m_Sigma.midboss_spread;
    if (spread == 0) {
        M_Add(x, y, group);
    } else {
        M_Add(Math16_Wrap((int32_t)x - spread), y, group);
        M_Add(x, Math16_Wrap((int32_t)y - spread), group);
        M_Add(Math16_Wrap((int32_t)x + spread), y, group);
        M_Add(x, Math16_Wrap((int32_t)y + spread), group);
    }
    m_Sigma.midboss_spread += 16;
}

static void M_Tiles(void)
{
    static const uint8_t images[] = { 0, 1, 2, 16, 17, 18, 9 };
    for (int i = 0; i < SIGMA_MIDBOSS_TILE_COUNT; i++) {
        SIGMA_MIDBOSS_TILE *const tile = &m_Sigma.tiles[i];
        if (tile->flag == 0) {
            continue;
        }
        tile->frame++;
        if ((tile->frame & 7) != 0) {
            continue;
        }
        Background_SetTile(
            g_Stage->scroll.front, g_Stage->scroll.back, tile->position.x,
            tile->position.y, images[tile->flag]);
        tile->flag++;
        if (tile->flag >= 7) {
            tile->position.y =
                Math16_Wrap((int32_t)tile->position.y - g_Stage->scroll.line);
            if (tile->position.y < 0) {
                tile->position.y = Math16_Wrap((int32_t)tile->position.y + 400);
            }

            Bullets_AddPellets(
                g_Stage->scroll.back,
                Math16_Wrap((int32_t)tile->position.x + 4),
                Math16_Wrap((int32_t)tile->position.y + 4), tile->angle,
                tile->group, tile->speed);
            tile->flag = 0;
        }
    }
}

static ENTITY_POINT *M_Position(BOSS_STATE *const boss)
{
    return &boss->position[g_Stage->scroll.back];
}

static int16_t M_PillarOffset(const bool advance)
{
    static const int16_t offsets[] = { 60, 44, 76, 28, 92 };
    const int index = m_Sigma.pillar_index;
    ASSERT(index >= 0 && (size_t)index < ARRAY_SIZE(offsets));
    if (advance) {
        m_Sigma.pillar_index++;
    }
    return offsets[index];
}

static ENTITY_POINT M_Aim(void)
{
    const MATH16_VECTOR target = {
        Math16_Wrap((int32_t)g_Player->current.x + 16),
        Math16_Wrap((int32_t)g_Player->current.y + 16),
    };
    const MATH16_VECTOR v = Math16_Aim(
        (MATH16_VECTOR) { m_Sigma.bullet_origin.x, m_Sigma.bullet_origin.y },
        target, 48, 0);
    return (ENTITY_POINT) { v.x, v.y };
}

static void M_OrbitCircle(const uint8_t color)
{
    Grcg_SetColor(color);
    Draw_Circle(m_Sigma.orbit_center.x, m_Sigma.orbit_center.y, 112);
    Grcg_Off();
}

static void M_SweepPellet(const uint8_t angle)
{
    const int16_t x =
        Math16_Wrap((int32_t)Random_RingU8And(0x7f) + m_Sigma.position.x);
    const int16_t speed = Random_RingU8And(0x1f) + 30;
    Bullets_AddPellets(
        g_Stage->scroll.back, x, Math16_Wrap((int32_t)m_Sigma.position.y + 64),
        angle, BULLET_SINGLE, speed);
}

static int M_CircleAdd(
    const int16_t x, const int16_t y, const int16_t radius_max)
{
    if (x > 0 && x < 444 && y > 0 && y < 400) {
        for (int i = 0; i < SIGMA_CIRCLE_COUNT; i++) {
            SIGMA_CIRCLE *const c = &m_Sigma.circles[i];
            if (c->flag != 0) {
                continue;
            }
            c->flag = 1;
            c->frame = 0;
            c->center = (ENTITY_POINT) { x, y };
            c->radius_max = radius_max;
            c->cel = 26;
            c->radius = 64;
            SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
            return 0;
        }
    }
    return 1;
}

static void M_Circles(const PATTERN_BANK *const patterns)
{
    const ENTITY_POINT player = g_Player->position[g_Stage->scroll.front];
    for (int i = 0; i < SIGMA_CIRCLE_COUNT; i++) {
        SIGMA_CIRCLE *const c = &m_Sigma.circles[i];
        if (c->flag == 0) {
            continue;
        }
        c->frame++;
        if (c->flag == 1) {
            if ((c->frame & m_Sigma.circle_frame_mask) == 0) {
                c->cel++;
                if (c->cel >= 34) {
                    c->flag++;
                    SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
                }
                c->frame = 0;
                c->radius -= 8;
                Grcg_SetColor(15);
                Draw_Circle(c->center.x, c->center.y, c->radius);
                Grcg_Off();
            }
            if (c->flag == 1) {
                const PATTERN *const pattern = &patterns->entries[c->cel];
                ASSERT(pattern->data != nullptr);
                Draw_SpriteRect(
                    pattern, Math16_Wrap((int32_t)c->center.x - 16),
                    Math16_Wrap((int32_t)c->center.y - 16));
            }
        } else if (c->flag == 3) {
            if (g_Stage->scroll.back == (i & 1)) {
                Grcg_SetColor(15);
                Draw_CircleFill(c->center.x, c->center.y, c->radius);
                Grcg_Off();
                if (c->radius < c->radius_max) {
                    c->radius += 16;
                }
            }
            if (c->frame > 32) {
                c->flag++;
                c->frame = 0;
                c->radius_max = c->radius;
                c->radius = 8;
            }
            const int shrink = m_Sigma.circle_hitbox_shrink;
            if (Math16_Wrap((int32_t)c->center.x - c->radius + shrink)
                    < player.x
                && Math16_Wrap((int32_t)c->center.x + c->radius - shrink - 32)
                    > player.x
                && Math16_Wrap((int32_t)c->center.y - c->radius + shrink)
                    < player.y
                && Math16_Wrap((int32_t)c->center.y + c->radius - shrink - 32)
                    > player.y) {
                Player_Hit();
            }
        } else if (c->flag == 4 && g_Stage->scroll.back == (i & 1)) {
            Grcg_SetColor(15);
            Draw_CircleFill(c->center.x, c->center.y, (uint16_t)c->radius_max);
            Grcg_Off();
            Grcg_SetColor(0);
            Draw_CircleFill(c->center.x, c->center.y, c->radius);
            Grcg_Off();
            if (c->radius >= c->radius_max) {
                c->flag = 0;
            } else {
                c->radius += 16;
            }
        }
    }
}

static void M_BodyHit(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    const int16_t sprite = boss->sprite;
    if (boss->defeated == 0) {
        const ENTITY_POINT back = *M_Position(boss);
        if (Boss_Hit(
                &boss->damage, Math16_Wrap((int32_t)back.x + 36),
                Math16_Wrap((int32_t)back.y + 32), 56, 32, 1, 1)
            != 0) {
            boss->sprite = 130;
        }
        const ENTITY_POINT player = g_Player->position[g_Stage->scroll.front];
        const ENTITY_POINT p = m_Sigma.position;
        if (player.x > Math16_Wrap((int32_t)p.x + 8)
            && player.x < Math16_Wrap((int32_t)p.x + 88) && player.y > p.y
            && player.y < Math16_Wrap((int32_t)p.y + 64)) {
            Player_Hit();
        }
    }
    Sigma_DrawBody(boss, patterns);
    boss->sprite = sprite;
}

static void M_ByteBox(
    const int16_t left, const int16_t top, const int16_t right,
    const int16_t bottom)
{
    const CLIP *const clip = g_Clip;
    int16_t first = Math16_Wrap((int32_t)top - clip->top);
    first = first > 0 ? first : 0;
    int16_t last = Math16_Wrap((int32_t)bottom - clip->top);
    const int16_t height = clip->bottom - clip->top;
    last = last < height ? last : height;
    const int16_t rows = Math16_Wrap((int32_t)last - first);
    const int16_t delta = Math16_Wrap((int32_t)right - left);
    if (rows < 0 || delta < 0) {
        return;
    }
    const uint16_t width = (uint16_t)delta + 1;
    const uint16_t subtract = width + 80;
    const uint32_t base = (clip->top + first) * 80;
    uint16_t offset = (uint16_t)((int32_t)rows * 80 + left);
    uint8_t *const page = g_Video.pages[g_Video.drawing_page];
    const uint8_t color = g_Grcg->tiles[0] != 0 ? 15 : 0;
    for (;;) {
        for (uint32_t col = 0; col < width; col++) {
            // RMW aliases the contiguous B/R/G address ranges.
            const uint32_t address = (base + offset) % VIDEO_PLANE_SIZE;
            memset(page + address * 8, color, 8);
            offset++;
        }
        // The original loop stops on subtraction borrow, not a row count.
        if (offset < subtract) {
            break;
        }
        offset -= subtract;
    }
}

static void M_BeamHit(void)
{
    const int16_t x = g_Player->position[g_Stage->scroll.front].x;
    if (x > m_Sigma.beam_left && x < m_Sigma.beam_right) {
        Player_Hit();
    }
}

static void M_Beam(const uint8_t color)
{
    Grcg_SetColor(color);
    M_ByteBox(m_Sigma.beam_byte_left, 16, m_Sigma.beam_byte_right, 383);
    Grcg_Off();
}

static bool M_Defeat(BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    const int16_t frame = boss->phase_frame;
    const ENTITY_POINT p = m_Sigma.position;
    if (frame < 10) {
        Sigma_DrawBody(boss, patterns);
        return false;
    }
    if (frame == 10) {
        m_Sigma.beam_width = 8;
        Sigma_DrawBody(boss, patterns);
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
    } else if (frame < 90) {
        Sigma_DrawBody(boss, patterns);
        if (g_Stage->scroll.back == 0) {
            Grcg_SetColor(15);
            Draw_CircleFill(
                Math16_Wrap((int32_t)p.x + 64), Math16_Wrap((int32_t)p.y + 64),
                24);
            Grcg_Off();
        }
        if (frame >= 50 && g_Stage->scroll.back != 0) {
            Grcg_SetColor(15);
            Draw_VLine(g_Clip, Math16_Wrap((int32_t)p.x + 64), 16, 383);
            Grcg_Off();
        }
        if (frame == 89) {
            SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        }
    } else if (frame < 100) {
        Grcg_SetColor(15);
        M_ByteBox(
            Math16_Wrap((int32_t)p.x + 64 - m_Sigma.beam_width) / 8, 16,
            Math16_Wrap((int32_t)p.x + 64 + m_Sigma.beam_width) / 8, 383);
        Grcg_Off();
        const int16_t x = g_Player->position[g_Stage->scroll.front].x;
        if (Math16_Wrap((int32_t)p.x + 56 - m_Sigma.beam_width) < x
            && x < Math16_Wrap((int32_t)p.x + 40 + m_Sigma.beam_width)) {
            Player_Hit();
        }
        m_Sigma.beam_width += 8;
        Sigma_DrawBody(boss, patterns);
    } else if (frame == 100) {
        m_Sigma.beam_left = Math16_Wrap((int32_t)p.x + 56 - m_Sigma.beam_width);
        m_Sigma.beam_right =
            Math16_Wrap((int32_t)p.x + 40 + m_Sigma.beam_width);
        m_Sigma.beam_byte_left =
            Math16_Wrap((int32_t)p.x + 64 - m_Sigma.beam_width) / 8;
        m_Sigma.beam_byte_right =
            Math16_Wrap((int32_t)p.x + 64 + m_Sigma.beam_width) / 8 - 1;
        boss->sprite = 134;
    } else if (frame <= 140) {
        M_Beam(15);
        Sigma_DrawBody(boss, patterns);
        M_BeamHit();
    } else if (frame < 180) {
        M_Beam(15);
        if (frame % 10 == 0) {
            boss->sprite = Math16_Wrap((int32_t)boss->sprite + 2);
        }
        Sigma_DrawBody(boss, patterns);
        M_BeamHit();
        const uint16_t offset = frame & 1;
        Video_SetSplit(
            400 - g_Stage->scroll.line, g_Stage->scroll.line * 40 + offset,
            offset);
        Stage_SetSlowdown(2);
    } else if (frame <= 220) {
        Stage_SetSlowdown(1);
        M_Beam(15);
        M_BeamHit();
    } else if (frame < 224) {
        M_Beam(0);
    } else {
        return true;
    }
    return false;
}

static void M_Sweep(BOSS_STATE *const boss)
{
    uint8_t angle;
    int i;

    if (boss->phase_frame < 20) {
        return;
    }
    if (boss->phase_frame == 20) {
        m_Sigma.sweep_frames = 0;
        m_Sigma.sweep_velocity = ((g_Player->current.x < 208) ? 1 : -1);
    }
    if ((boss->phase_frame % 4) == 0) {
        SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
    }
    if (boss->phase_frame < 148) {
        M_Position(boss)->x = Math16_Wrap(
            (int32_t)M_Position(boss)->x + (m_Sigma.sweep_velocity));
        if (boss->phase_frame & 1) {
            M_SweepPellet(0x40);
        }
    } else if (boss->phase_frame < 212) {
        M_Position(boss)->x = Math16_Wrap(
            (int32_t)M_Position(boss)->x - ((m_Sigma.sweep_velocity * 4)));
        if (boss->phase_frame & 1) {
            M_SweepPellet(0x40);
        }
    } else if (boss->phase_frame < 340) {
        M_Position(boss)->x = Math16_Wrap(
            (int32_t)M_Position(boss)->x + (m_Sigma.sweep_velocity));
        if (boss->phase_frame & 1) {
            M_SweepPellet(0x40);
        }
    } else if (boss->phase_frame < 440) {
        m_Sigma.sweep_frames++;
        ASSERT(m_Sigma.sweep_frames != 0);
        angle =
            ((Random_RingU8() % (m_Sigma.sweep_frames * 2))
             + (0x40 - m_Sigma.sweep_frames));
        if ((boss->phase_frame & 3) == 0) {
            for (i = 0; i < 3; i++) {
                M_SweepPellet(angle);
            }
        }
    } else {
        boss->phase_frame = 0;
    }
}

static void M_LowerCircles(BOSS_STATE *const boss)
{
    if (boss->phase_frame < 100) {
        return;
    }
    if (boss->phase_frame == 100) {
        m_Sigma.circle_frame_mask = 7;
        m_Sigma.circle_hitbox_shrink = 4;
        M_CircleAdd(128, 320, 64);
    } else if (boss->phase_frame == 120) {
        M_CircleAdd(320, 320, 64);
    } else if (
        (boss->phase_frame > 130) && (boss->phase_frame < 150)
        && ((boss->phase_frame % 4) == 0)) {
        Bullets_AddPellets(
            g_Stage->scroll.back, m_Sigma.bullet_origin.x,
            m_Sigma.bullet_origin.y, 0x00, BULLET_FAN5_MEDIUM_AIMED,
            ((7 << 4) + 8));
    }
    if (boss->phase_frame > 200) {
        boss->phase_frame = 0;
    }
}

static bool M_Drift(BOSS_STATE *const boss)
{
    if (boss->phase_frame < 50) {
        return true;
    }
    if (boss->phase_frame == 50) {
        m_Sigma.drift_velocity = ((g_Player->current.x < 208) ? 1 : -1);
    }
    if (boss->phase_frame < 114) {
        M_Position(boss)->x = Math16_Wrap(
            (int32_t)M_Position(boss)->x + (m_Sigma.drift_velocity));
        M_Position(boss)->y = Math16_Wrap((int32_t)M_Position(boss)->y + 1);
    } else if (boss->phase_frame < 242) {
        M_Position(boss)->x = Math16_Wrap(
            (int32_t)M_Position(boss)->x - (m_Sigma.drift_velocity));
    } else if (boss->phase_frame < 306) {
        M_Position(boss)->x = Math16_Wrap(
            (int32_t)M_Position(boss)->x + (m_Sigma.drift_velocity));
        M_Position(boss)->y = Math16_Wrap((int32_t)M_Position(boss)->y - 1);
    } else {
        boss->phase_frame = 0;
    }
    return false;
}

static void M_RingDrift(BOSS_STATE *const boss)
{
    if (M_Drift(boss)) {
        return;
    }
    if ((boss->phase_frame & 0x1F) == 0) {
        Bullets_AddPellets(
            g_Stage->scroll.back, m_Sigma.bullet_origin.x,
            m_Sigma.bullet_origin.y, Random_RingU8(), BULLET_RING32,
            ((5 << 4) + 5));
    }
}

static void M_OrbitCircles(BOSS_STATE *const boss)
{
    uint8_t angle;
    int16_t left;
    int16_t top;

    if (boss->phase_frame < 50) {
        return;
    }
    if (boss->phase_frame == 50) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
    }
    if (boss->phase_frame < 100) {
        boss->sprite = ((g_Stage->scroll.back * 4) + 128);
        return;
    }
    if (boss->phase_frame == 100) {
        m_Sigma.orbit_center.x = Math16_Wrap((int32_t)g_Player->current.x + 16);
        m_Sigma.orbit_center.y = Math16_Wrap((int32_t)g_Player->current.y + 16);
        m_Sigma.circle_frame_mask = 3;
        m_Sigma.circle_hitbox_shrink = -4;
        m_Sigma.orbit_angle = 0x00;
        boss->sprite = 128;
    }
    if (boss->phase_frame < 130) {
        if ((boss->phase_frame % 8) == 0) {
            Grcg_SetColor(15);
            Draw_Circle(m_Sigma.orbit_center.x, m_Sigma.orbit_center.y, 112);
        } else if ((boss->phase_frame % 8) == 2) {
            Grcg_SetColor(0);
            Draw_Circle(m_Sigma.orbit_center.x, m_Sigma.orbit_center.y, 112);
        }
        Grcg_Off();
    } else if (boss->phase_frame < 450) {
        if (g_Stage->scroll.back == 0) {
            M_OrbitCircle(15);
        }
        if (boss->phase_frame >= 150) {
            if ((boss->phase_frame % 12) == 0) {
                left = Math16_PolarX(
                    m_Sigma.orbit_center.x, 112, m_Sigma.orbit_angle);
                top = Math16_PolarY(
                    m_Sigma.orbit_center.y, 112, m_Sigma.orbit_angle);
                M_CircleAdd(left, top, 24);
                angle = (m_Sigma.orbit_angle + 0x80);
                left = Math16_PolarX(m_Sigma.orbit_center.x, 112, angle);
                top = Math16_PolarY(m_Sigma.orbit_center.y, 112, angle);
                M_CircleAdd(left, top, 24);
                m_Sigma.orbit_angle -= 0x10;
            }
            if ((boss->phase_frame >= 200)
                && ((boss->phase_frame & 0xF) == 0)) {
                Bullets_AddPellets(
                    g_Stage->scroll.back, m_Sigma.bullet_origin.x,
                    m_Sigma.bullet_origin.y, Random_RingU8(), BULLET_RING32,
                    (5 << 4));
            }
        }
    } else if (g_Stage->scroll.back == 0) {
        M_OrbitCircle(0);
        boss->phase_frame = 0;
    }
}

static void M_RapidDrift(BOSS_STATE *const boss)
{
    if (M_Drift(boss)) {
        return;
    }
    if ((boss->phase_frame & 7) == 0) {
        Bullets_AddPellets(
            g_Stage->scroll.back, m_Sigma.bullet_origin.x,
            m_Sigma.bullet_origin.y, Random_RingU8(), BULLET_RING16,
            ((3 << 4) + 2));
    }
}

static void M_BounceStream(BOSS_STATE *const boss)
{
    if (boss->phase_frame < 50) {
        return;
    }
    if (boss->phase_frame == 50) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
    }
    if (boss->phase_frame < 100) {
        boss->sprite = ((g_Stage->scroll.back * 4) + 128);
        return;
    }
    if (boss->phase_frame == 100) {
        boss->sprite = 128;
        m_Sigma.bounce_left = 32;
        m_Sigma.bounce_round = 0;
        Bullets_SetTurnLimit(1);
    }
    if ((boss->phase_frame & 0xF) == 0) {
        Bullets_AddSprites(
            g_Stage->scroll.back, m_Sigma.bounce_left, (16 + 8), 0x40,
            BULLET_MOTION_BOUNCE, (m_Sigma.bounce_round + 122), (4 << 4));
        Bullets_AddSprites(
            g_Stage->scroll.back, ((416 + 16) - m_Sigma.bounce_left), (16 + 8),
            0x40, BULLET_MOTION_BOUNCE, (m_Sigma.bounce_round + 122), (4 << 4));
        m_Sigma.bounce_left = Math16_Wrap((int32_t)m_Sigma.bounce_left + (16));
        if (m_Sigma.bounce_left == (32 + (384 / 2))) {
            Pillars_SetSpawnInterval(32);
            Pillars_Add(
                (m_Sigma.position.x + 60), m_Sigma.bullet_origin.y, 1, 0x6F);
            Pillars_SetSpawnInterval(48);
            Pillars_Add(
                (m_Sigma.position.x + 44), m_Sigma.bullet_origin.y, 1, 0x6F);
            Pillars_Add(
                (m_Sigma.position.x + 76), m_Sigma.bullet_origin.y, 1, 0x6F);
            Pillars_SetSpawnInterval(100);
            Pillars_Add(
                (m_Sigma.position.x + 28), m_Sigma.bullet_origin.y, 1, 0x6F);
            Pillars_Add(
                (m_Sigma.position.x + 92), m_Sigma.bullet_origin.y, 1, 0x6F);
            Pillars_SetSpawnInterval(16);
        }
        if (m_Sigma.bounce_left >= (416 + 16)) {
            m_Sigma.bounce_left = 32;
            m_Sigma.bounce_round++;
            if (m_Sigma.bounce_round >= 2) {
                boss->phase_frame = 0;
            }
        }
    }
}

static void M_PillarFan(BOSS_STATE *const boss)
{
    uint8_t angle;
    int i;

    if (M_Drift(boss)) {
        return;
    }
    if (boss->phase_frame == 100) {
        m_Sigma.pillar_index = 0;
    } else if (boss->phase_frame == 114) {
        Pillars_Add(
            (M_PillarOffset(true) + m_Sigma.position.x),
            m_Sigma.bullet_origin.y, 1, 0x6F);
    } else if (boss->phase_frame == 118) {
        Pillars_Add(
            (M_PillarOffset(true) + m_Sigma.position.x),
            m_Sigma.bullet_origin.y, 1, 0x6F);
        Pillars_Add(
            (M_PillarOffset(true) + m_Sigma.position.x),
            m_Sigma.bullet_origin.y, 1, 0x6F);
    } else if (boss->phase_frame == 122) {
        Pillars_Add(
            (M_PillarOffset(true) + m_Sigma.position.x),
            m_Sigma.bullet_origin.y, 1, 0x6F);
        Pillars_Add(
            (M_PillarOffset(false) + m_Sigma.position.x),
            m_Sigma.bullet_origin.y, 1, 0x6F);
        m_Sigma.pillar_index = 0;
    } else if (boss->phase_frame == 242) {
        Pillars_Add(
            (M_PillarOffset(true) + m_Sigma.position.x),
            m_Sigma.bullet_origin.y, 1, 0x6F);
    } else if (boss->phase_frame == 246) {
        Pillars_Add(
            (M_PillarOffset(true) + m_Sigma.position.x),
            m_Sigma.bullet_origin.y, 1, 0x6F);
        Pillars_Add(
            (M_PillarOffset(true) + m_Sigma.position.x),
            m_Sigma.bullet_origin.y, 1, 0x6F);
    } else if (boss->phase_frame == 250) {
        Pillars_Add(
            (M_PillarOffset(true) + m_Sigma.position.x),
            m_Sigma.bullet_origin.y, 1, 0x6F);
        Pillars_Add(
            (M_PillarOffset(false) + m_Sigma.position.x),
            m_Sigma.bullet_origin.y, 1, 0x6F);
        m_Sigma.pillar_index = 0;
    } else if (
        (boss->phase_frame > 120) && (boss->phase_frame < 240)
        && ((boss->phase_frame & 0xF) == 0)) {
        for ((angle = 0x90, i = 0); angle < 0xF0; (angle += 0x10, i++)) {
            Bullets_AddSprites(
                g_Stage->scroll.back, m_Sigma.bullet_origin.x,
                m_Sigma.bullet_origin.y, angle, BULLET_MOTION_BOUNCE,
                ((i % 2) + 122), ((3 << 4) + 12));
        }
    }
}

static void M_TrackingCircles(BOSS_STATE *const boss)
{
    int first_failed;

    if (boss->phase_frame < 50) {
        return;
    }
    if (boss->phase_frame == 50) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
    }
    if (boss->phase_frame < 100) {
        boss->sprite = ((g_Stage->scroll.back * 4) + 128);
        return;
    }
    if (boss->phase_frame == 100) {
        m_Sigma.tracking_velocity = M_Aim();
        m_Sigma.tracking_other_velocity =
            Math16_Wrap(-(int32_t)m_Sigma.tracking_velocity.x);
        boss->sprite = 128;
        m_Sigma.tracking_left = m_Sigma.bullet_origin.x;
        m_Sigma.tracking_other_left = m_Sigma.bullet_origin.x;
        m_Sigma.circle_hitbox_shrink = -4;
        m_Sigma.circle_frame_mask = 3;
        m_Sigma.tracking_top = m_Sigma.bullet_origin.y;
    }
    if ((boss->phase_frame & 7) == 0) {
        first_failed =
            M_CircleAdd(m_Sigma.tracking_left, m_Sigma.tracking_top, 48);
        if (M_CircleAdd(m_Sigma.tracking_other_left, m_Sigma.tracking_top, 48)
            && first_failed) {
            boss->phase_frame = 0;
        }
        m_Sigma.tracking_left = Math16_Wrap(
            (int32_t)m_Sigma.tracking_left + (m_Sigma.tracking_velocity.x));
        m_Sigma.tracking_other_left = Math16_Wrap(
            (int32_t)m_Sigma.tracking_other_left
            + (m_Sigma.tracking_other_velocity));
        m_Sigma.tracking_top = Math16_Wrap(
            (int32_t)m_Sigma.tracking_top + (m_Sigma.tracking_velocity.y));
        if (Math16_Wrap((int32_t)g_Player->current.x + 16)
            > m_Sigma.tracking_left) {
            m_Sigma.tracking_left =
                Math16_Wrap((int32_t)m_Sigma.tracking_left + (16));
        } else if (
            Math16_Wrap((int32_t)g_Player->current.x + 16)
            < m_Sigma.tracking_left) {
            m_Sigma.tracking_left =
                Math16_Wrap((int32_t)m_Sigma.tracking_left - (16));
        }
        if (Math16_Wrap((int32_t)g_Player->current.x + 16)
            > m_Sigma.tracking_other_left) {
            m_Sigma.tracking_other_left =
                Math16_Wrap((int32_t)m_Sigma.tracking_other_left - (16));
        } else if (
            Math16_Wrap((int32_t)g_Player->current.x + 16)
            < m_Sigma.tracking_other_left) {
            m_Sigma.tracking_other_left =
                Math16_Wrap((int32_t)m_Sigma.tracking_other_left + (16));
        }
        Bullets_AddPellets(
            g_Stage->scroll.back, m_Sigma.bullet_origin.x,
            m_Sigma.bullet_origin.y, Random_RingU8(), BULLET_RING16,
            ((3 << 4) + 12));
    }
}

static void M_AimedCircles(BOSS_STATE *const boss)
{
    if (M_Drift(boss)) {
        return;
    }
    if (boss->phase_frame == 50) {
        m_Sigma.circle_frame_mask = 7;
    }
    if ((boss->phase_frame & 0x3F) == 32) {
        M_CircleAdd(
            Math16_Wrap((int32_t)g_Player->current.x + 16),
            Math16_Wrap((int32_t)g_Player->current.y + 16), 48);
    }
    if ((boss->phase_frame & 0x3F) == 0) {
        Bullets_AddPellets(
            g_Stage->scroll.back, m_Sigma.bullet_origin.x,
            m_Sigma.bullet_origin.y, Random_RingU8(), BULLET_RING32,
            ((3 << 4) + 12));
    }
}

static bool M_WideDrift(BOSS_STATE *const boss)
{
    if (boss->phase_frame < 50) {
        return true;
    }
    if (boss->phase_frame == 50) {
        m_Sigma.wide_velocity = ((g_Player->current.x < 208) ? 2 : -2);
    }
    if (boss->phase_frame < 130) {
        M_Position(boss)->x =
            Math16_Wrap((int32_t)M_Position(boss)->x + (m_Sigma.wide_velocity));
    } else if (boss->phase_frame < 290) {
        M_Position(boss)->x =
            Math16_Wrap((int32_t)M_Position(boss)->x - (m_Sigma.wide_velocity));
    } else if (boss->phase_frame < 370) {
        M_Position(boss)->x =
            Math16_Wrap((int32_t)M_Position(boss)->x + (m_Sigma.wide_velocity));
    } else {
        boss->phase_frame = 0;
    }
    return false;
}

static void M_FanSweep(BOSS_STATE *const boss)
{
    if (M_WideDrift(boss)) {
        return;
    }
    if (boss->phase_frame == 50) {
        m_Sigma.circle_frame_mask = 7;
    }
    if ((boss->phase_frame & 0x3F) == 0) {
        M_CircleAdd(
            Math16_Wrap((int32_t)g_Player->current.x + 16),
            Math16_Wrap((int32_t)g_Player->current.y + 16), 48);
    }
    if ((boss->phase_frame & 7) == 0) {
        Bullets_AddPellets(
            g_Stage->scroll.back, m_Sigma.bullet_origin.x,
            m_Sigma.bullet_origin.y, 0x00, BULLET_FAN2_EXTRA_WIDE_AIMED,
            ((4 << 4) + 6));
    }
}

static void M_BallSweep(BOSS_STATE *const boss)
{
    if (M_WideDrift(boss)) {
        return;
    }
    if (boss->phase_frame == 50) {
        m_Sigma.ball_angle = 0x00;
    }
    if ((boss->phase_frame & 7) == 0) {
        Bullets_AddSprites(
            g_Stage->scroll.back, m_Sigma.bullet_origin.x,
            m_Sigma.bullet_origin.y, m_Sigma.ball_angle, BULLET_RING8, 86,
            (5 << 4));
        m_Sigma.ball_angle += 0x08;
    }
}

static void M_AlternateCircles(BOSS_STATE *const boss)
{
    if (M_WideDrift(boss)) {
        return;
    }
    if (boss->phase_frame == 50) {
        m_Sigma.circle_frame_mask = 7;
    }
    if ((boss->phase_frame & 0x1F) == 0) {
        M_CircleAdd(
            Math16_Wrap((int32_t)g_Player->current.x + 16),
            Math16_Wrap((int32_t)g_Player->current.y + 16), 48);
    }
    if ((boss->phase_frame & 0x1F) == 16) {
        Bullets_AddPellets(
            g_Stage->scroll.back, m_Sigma.bullet_origin.x,
            m_Sigma.bullet_origin.y, Random_RingU8(), BULLET_RING16,
            ((3 << 4) + 12));
    }
}

static void M_RunAttack(BOSS_STATE *const boss, const SIGMA_ATTACK attack)
{
    static const SIGMA_ATTACK_UPDATE strategies[] = {
        [SIGMA_ATTACK_SWEEP] = M_Sweep,
        [SIGMA_ATTACK_LOWER_CIRCLES] = M_LowerCircles,
        [SIGMA_ATTACK_RING_DRIFT] = M_RingDrift,
        [SIGMA_ATTACK_ORBIT_CIRCLES] = M_OrbitCircles,
        [SIGMA_ATTACK_RAPID_DRIFT] = M_RapidDrift,
        [SIGMA_ATTACK_BOUNCE_STREAM] = M_BounceStream,
        [SIGMA_ATTACK_PILLAR_FAN] = M_PillarFan,
        [SIGMA_ATTACK_TRACKING_CIRCLES] = M_TrackingCircles,
        [SIGMA_ATTACK_AIMED_CIRCLES] = M_AimedCircles,
        [SIGMA_ATTACK_FAN_SWEEP] = M_FanSweep,
        [SIGMA_ATTACK_BALL_SWEEP] = M_BallSweep,
        [SIGMA_ATTACK_ALTERNATE_CIRCLES] = M_AlternateCircles,
    };
    ASSERT(
        (unsigned)attack < ARRAY_SIZE(strategies)
        && strategies[attack] != nullptr);
    strategies[attack](boss);
}

static void M_Patterns(BOSS_STATE *const boss, const uint8_t count)
{
    if (m_Sigma.pattern < 3) {
        M_RunAttack(boss, m_Sigma.attacks[m_Sigma.pattern]);
    }
    if (boss->phase_frame == 0) {
        m_Sigma.pattern++;
        if (m_Sigma.pattern >= count) {
            m_Sigma.pattern = 0;
            m_Sigma.patterns_done = 1;
        }
        if (boss->damage >= m_Sigma.phase_damage) {
            Boss_ResetDamage(
                &boss->damage, m_Sigma.phase == SIGMA_TRACKING ? 1300 : 1800);
            m_Sigma.phase++;
            m_Sigma.pattern = 0;
            m_Sigma.patterns_done = 0;
        }
    }
}

static void M_StartPhase(
    BOSS_STATE *const boss, const SIGMA_ATTACK first, const SIGMA_ATTACK second)
{
    m_Sigma.pattern = 0;
    boss->phase_frame = 0;
    m_Sigma.phase++;
    m_Sigma.patterns_done = 0;
    m_Sigma.attacks[0] = first;
    m_Sigma.attacks[1] = second;
}

void Sigma_Phases(BOSS_STATE *const boss)
{
    switch (m_Sigma.phase) {
    case SIGMA_APPEAR:
        if (boss->phase_frame > 50) {
            M_StartPhase(boss, SIGMA_ATTACK_SWEEP, SIGMA_ATTACK_LOWER_CIRCLES);
            m_Sigma.attacks[2] = SIGMA_ATTACK_RING_DRIFT;
            m_Sigma.phase_damage = 1800;
        }
        break;
    case SIGMA_OPENING:
        M_Patterns(boss, 3);
        break;
    case SIGMA_PREPARE_ORBIT:
        M_StartPhase(
            boss, SIGMA_ATTACK_ORBIT_CIRCLES, SIGMA_ATTACK_RAPID_DRIFT);
        m_Sigma.phase_damage = 1800;
        break;
    case SIGMA_ORBIT:
        M_Patterns(boss, 2);
        break;
    case SIGMA_PREPARE_BOUNCE:
        M_StartPhase(boss, SIGMA_ATTACK_BOUNCE_STREAM, SIGMA_ATTACK_PILLAR_FAN);
        m_Sigma.phase_damage = 1800;
        break;
    case SIGMA_BOUNCE:
        M_Patterns(boss, 2);
        break;
    case SIGMA_PREPARE_TRACKING:
        M_StartPhase(
            boss, SIGMA_ATTACK_TRACKING_CIRCLES, SIGMA_ATTACK_AIMED_CIRCLES);
        m_Sigma.phase_damage = 1800;
        break;
    case SIGMA_TRACKING:
        M_Patterns(boss, 2);
        break;
    case SIGMA_PREPARE_FINAL:
        M_StartPhase(boss, SIGMA_ATTACK_FAN_SWEEP, SIGMA_ATTACK_BALL_SWEEP);
        m_Sigma.attacks[2] = SIGMA_ATTACK_ALTERNATE_CIRCLES;
        m_Sigma.phase_damage = 5000;
        break;
    case SIGMA_FINAL:
        M_Patterns(boss, 3);
        if (boss->damage >= 1300) {
            boss->defeated = 1;
            Score_AddBonus(300000);
            boss->phase_frame = 0;
        }
        break;
    }
}

static void M_Rings(const bool reduce_effects)
{
    m_Sigma.ring_angle += 8;
    Grcg_SetColor(4);
    const unsigned step = reduce_effects ? 16 : 2;
    uint8_t radius = m_Sigma.ring_angle;
    for (int i = 0; i < 2; i++, radius += 128) {
        for (unsigned angle = 0; angle < 256; angle += step) {
            const int16_t x = Math16_PolarX(224, radius, angle);
            const int16_t y = Math16_PolarY(200, radius, angle);
            if (x > 24 && x < 416 && y > 8 && y < 384) {
                int16_t rolled = Math16_Wrap((int32_t)y + g_Stage->scroll.line);
                if (rolled >= 400) {
                    rolled -= 400;
                }
                Draw_Dot(x, rolled, (radius >> 6) + 1);
            }
        }
    }
    Grcg_Off();
}

static void M_Init(void)
{
    memset(&m_Sigma, 0, sizeof(m_Sigma));
}

bool Sigma_MidbossInvalidate(void)
{
    return m_Sigma.midboss_active;
}

void Sigma_MidbossUpdate(BOSS_STATE *const boss)
{
    boss->phase_frame = Math16_Wrap((int32_t)boss->phase_frame + 1);
    const int16_t frame = boss->phase_frame;
    if (frame == 1) {
        m_Sigma.midboss_spread = 0;
        m_Sigma.midboss_active = true;
    } else if (frame < 96) {
        if (frame % 16 == 0) {
            M_Cross(128, 128, 0x21);
        }
    } else if (frame == 96) {
        m_Sigma.midboss_spread = 0;
    } else if (frame < 192) {
        if (frame % 16 == 0) {
            M_Cross(304, 0, 0x24);
        }
    } else if (frame == 192) {
        m_Sigma.midboss_spread = 0;
    } else if (frame < 288 && frame % 16 == 0) {
        M_Cross(192, 336, 0x25);
    }
    if (frame > 420) {
        boss->phase_frame = 0;
        m_Sigma.midboss_active = false;
    }
    M_Tiles();
}

void Sigma_SelectPatterns(
    PATTERN_BANK *const patterns, const SIGMA_ASSETS *const assets)
{
    if (assets == nullptr) {
        return;
    }
    Boss_SelectPatterns(
        patterns, &assets->patterns,
        assets->has_palette ? assets->palette : nullptr);
}

void Sigma_Begin(BOSS_STATE *const boss)
{
    boss->position[0] = boss->position[1] = m_Sigma.position =
        (ENTITY_POINT) { 160, 48 };
    Boss_ResetDamage(&boss->damage, 1800);
    boss->phase_frame = 0;
    boss->sprite = 128;
    m_Sigma.phase = SIGMA_APPEAR;
    m_Sigma.circle_frame_mask = 7;
    Bullets_RemoveAll(StageScroll_Pages());
    Shots_FreeAll();
}

void Sigma_DrawBody(
    const BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    for (int part = 0; part < 2; part++) {
        const int id = boss->sprite + part;
        ASSERT(id >= 0 && id < PATTERN_LIMIT);
        const PATTERN *const pattern = &patterns->entries[id];
        ASSERT(pattern->data != nullptr);
        Draw_SpriteRect(
            pattern, Math16_Wrap((int32_t)m_Sigma.position.x + part * 64),
            m_Sigma.position.y);
    }
}

void Sigma_Invalidate(BOSS_STATE *const boss)
{
    boss->position[g_Stage->scroll.back] =
        boss->position[g_Stage->scroll.front];
    const CLIP full = { 0, 0, 639, 399 };
    Grcg_SetColor(0);
    Draw_BoxFill(&full, 32, 16, 415, 383);
    Grcg_Off();
    for (int i = 0; i < SIGMA_CIRCLE_COUNT; i++) {
        if (m_Sigma.circles[i].flag == 2) {
            m_Sigma.circles[i].flag++;
        }
    }
}

BOSS_RESULT Sigma_Update(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns,
    const bool reduce_effects)
{
    m_Sigma.position = *M_Position(boss);
    m_Sigma.bullet_origin = (ENTITY_POINT) {
        Math16_Wrap((int32_t)m_Sigma.position.x + 60),
        Math16_Wrap((int32_t)m_Sigma.position.y + 60),
    };
    boss->phase_frame = Math16_Wrap((int32_t)boss->phase_frame + 1);
    Shots_SetTarget(m_Sigma.bullet_origin);
    if ((g_Stage->world.frame & 1) == 0) {
        M_Rings(reduce_effects);
    }
    if (boss->defeated != 0) {
        if (M_Defeat(boss, patterns)) {
            return BOSS_FINISHED;
        }
    } else {
        Sigma_Phases(boss);
        m_Sigma.position = *M_Position(boss);
        M_BodyHit(boss, patterns);
        Stage_CountExtraBossFrame();
    }
    M_Circles(patterns);
    return BOSS_RUNNING;
}

// Stage 6's bare white room: no ordinary intro box, so the talk flow clears
// the tile background and selects Sigma's patterns before her reveal.
static void M_TalkSelectPatterns(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const int bank)
{
    (void)bank;
    Sigma_SelectPatterns(
        patterns, assets->sigma.patterns.next != 0 ? &assets->sigma : nullptr);
}

static void M_Begin(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const TH02_SESSION *const session)
{
    if (assets->stages[session->stage].boxes == nullptr) {
        Sigma_SelectPatterns(
            patterns,
            assets->sigma.patterns.next != 0 ? &assets->sigma : nullptr);
        Clip_SetPlayfield();
        Sigma_Begin(p_BossState);
    }
}

static void M_Invalidate(const bool reduce_effects)
{
    Sigma_Invalidate(p_BossState);
}

static BOSS_RESULT M_Update(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, TH02_SESSION *const session,
    const TH02_INPUT *const input)
{
    return Sigma_Update(p_BossState, patterns, session->reduce_effects);
}

static bool M_MidbossInvalidate(void)
{
    return Sigma_MidbossInvalidate();
}

static void M_MidbossUpdate(
    const PATTERN_BANK *const patterns, const uint8_t rank)
{
    Sigma_MidbossUpdate(p_BossState);
}

const BOSS_STRATEGY g_BossSigma = {
    .init = M_Init,
    .begin = M_Begin,
    .invalidate = M_Invalidate,
    .update = M_Update,
    .post_dialogue = true,
    .midboss_invalidate = M_MidbossInvalidate,
    .midboss_update = M_MidbossUpdate,
    .talk = { .place = Sigma_Begin,
              .draw_scene = Sigma_DrawBody,
              .select_patterns = M_TalkSelectPatterns,
              .extra_background = true },
};
