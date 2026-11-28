#include "th02/boss/meira.h"

#include "th02/background.h"
#include "th02/boss/explosion.h"
#include "th02/boss/private.h"
#include "th02/boss/private_meira.h"
#include "th02/boss/strategy.h"
#include "th02/bullets.h"
#include "th02/player.h"
#include "th02/random.h"
#include "th02/score.h"
#include "th02/shots.h"
#include "th02/sound_effects.h"
#include "th02/sparks.h"
#include "th02/stage.h"
#include "th02/stage_effects.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

typedef void (*MEIRA_ATTACK)(BOSS_STATE *boss, const PATTERN_BANK *patterns);

static MEIRA m_Meira;
MEIRA *const p_Meira = &m_Meira;
static MEIRA_MIDBOSS m_Midboss;
MEIRA_MIDBOSS *const p_MeiraMidboss = &m_Midboss;

static const PATTERN *M_Pattern(
    const PATTERN_BANK *const patterns, const int16_t id)
{
    ASSERT(id >= 0 && id < PATTERN_LIMIT);
    const PATTERN *const pattern = &patterns->entries[id];
    ASSERT(pattern->data != nullptr);
    return pattern;
}

static void M_Flash(
    const BOSS_STATE *const boss, const PATTERN_BANK *const patterns,
    const int16_t top)
{
    SoundEffects_Play(SOUND_EFFECT_HIT);
    Draw_SpriteRollPlane(
        M_Pattern(patterns, 136), boss->position[g_Stage->scroll.back].x, top,
        0, 0xffc0);
}

static int16_t M_RolledTop(const int16_t top)
{
    int16_t rolled = Math16_Wrap((int32_t)top + g_Stage->scroll.line);
    if (rolled < 0) {
        rolled = Math16_Wrap((int32_t)rolled + 400);
    } else if (rolled >= 400) {
        rolled -= 400;
    }
    return rolled;
}

static void M_MidbossDefeat(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    MEIRA_MIDBOSS *const midboss = &m_Midboss;
    const ENTITY_POINT position = boss->position[g_Stage->scroll.back];
    if (midboss->kill_frame < 48 && (midboss->kill_frame & 15) == 5) {
        Sparks_Add(
            g_Stage->scroll.back, Math16_Wrap((int32_t)position.x + 32),
            (uint16_t)(position.y + 32), 128, 24, true);
    }
    const int16_t image = 10 + (midboss->kill_frame >> 3);
    midboss->kill_frame = Math16_Wrap((int32_t)midboss->kill_frame + 1);
    if (midboss->kill_frame >= 64) {
        midboss->kill_frame = 0;
        boss->phase_frame = 0;
        boss->defeated = 0;
        midboss->active = false;
        Shots_ResetTarget();
        return;
    }
    int16_t top = Math16_Wrap((int32_t)position.y + g_Stage->scroll.line);
    if (top >= 400) {
        top -= 400;
    }
    Draw_SpriteZoom(M_Pattern(patterns, image), position.x, top, 2);
}

static void M_MidbossFire(const BOSS_STATE *const boss)
{
    const int16_t frame = boss->phase_frame;
    if (frame % 8 != 0 || frame % 192 >= 64) {
        return;
    }
    const ENTITY_POINT position = boss->position[g_Stage->scroll.back];

    Bullets_AddSprites(
        g_Stage->scroll.back, Math16_Wrap((int32_t)position.x + 28),
        Math16_Wrap((int32_t)position.y + 32), (uint8_t)(frame * 4),
        BULLET_MOTION_CHASE, 84, 30);
}

static ENTITY_POINT *M_Position(BOSS_STATE *const boss)
{
    return &boss->position[g_Stage->scroll.back];
}

static int16_t M_Top(const int16_t top)
{
    int16_t rolled = Math16_Wrap((int32_t)top + g_Stage->scroll.line);
    if (rolled >= 400) {
        rolled -= 400;
    }
    return rolled;
}

static void M_Billiards(BOSS_STATE *const boss, const int count)
{
    const ENTITY_POINT position = *M_Position(boss);

    for (int i = 0; i < count; i++) {
        const uint8_t angle = Random_RingU8();
        Bullets_AddSprites(
            g_Stage->scroll.back, Math16_Wrap((int32_t)position.x + 24),
            Math16_Wrap((int32_t)position.y + 24), angle, BULLET_MOTION_BOUNCE,
            122 + (i & 1), 54);
    }
}

static void M_FlowerAdd(const int16_t left, const int16_t top)
{
    MEIRA *const meira = &m_Meira;
    for (size_t i = 0; i < MEIRA_FLOWER_COUNT; i++) {
        MEIRA_FLOWER *const p = &meira->flowers[i];
        if (p->flag != ENTITY_FREE) {
            continue;
        }
        p->flag = ENTITY_ACTIVE;
        p->group = meira->flower_group;
        p->position = (ENTITY_POINT) { left, top };
        p->angle = 0;
        p->frame = 0;
        p->speed = meira->flower_speed;
        p->sprite = meira->flower_sprite;
        p->frames_left = (uint8_t)meira->flower_frames;
        return;
    }
}

static void M_Flowers(const PATTERN_BANK *const patterns, const uint8_t rank)
{
    MEIRA *const meira = &m_Meira;
    const ENTITY_POINT player = g_Player->current;
    for (size_t i = 0; i < MEIRA_FLOWER_COUNT; i++) {
        MEIRA_FLOWER *const p = &meira->flowers[i];
        if (p->flag != ENTITY_ACTIVE) {
            continue;
        }
        if (p->frames_left > 0) {
            Draw_SpriteRollTiny(
                M_Pattern(patterns, p->sprite), p->position.x,
                M_Top(p->position.y));
            p->frames_left--;
            if (Math16_Wrap((int32_t)p->position.x - 16) <= player.x
                && p->position.x >= player.x
                && Math16_Wrap((int32_t)p->position.y - 16) < player.y
                && p->position.y > player.y) {
                Player_Hit();
            }
            continue;
        }
        if (p->frame == 0) {
            SoundEffects_Play(SOUND_EFFECT_ATTACK_RELEASE);
            p->position.x = Math16_Wrap((int32_t)p->position.x - 8);
            p->position.y = Math16_Wrap((int32_t)p->position.y - 8);
        } else if (p->frame >= 32) {
            if ((meira->flower_fire_i & 1) <= rank) {
                Bullets_AddPellets(
                    g_Stage->scroll.back,
                    Math16_Wrap((int32_t)p->position.x + 12),
                    Math16_Wrap((int32_t)p->position.y + 12), p->angle,
                    p->group, p->speed);
            }
            meira->flower_fire_i++;
            p->flag = ENTITY_REMOVE;
            continue;
        }
        Draw_SpriteRoll(
            M_Pattern(patterns, (p->frame >> 2) + 26), p->position.x,
            M_Top(p->position.y));
        p->frame++;
    }
}

static void M_Move(BOSS_STATE *const boss, const int16_t dx, const int16_t dy)
{
    ENTITY_POINT *const position = M_Position(boss);
    if (m_Meira.direction == 0) {
        position->x = Math16_Wrap((int32_t)position->x + dx);
        if (position->x > 368) {
            position->x = 368;
        }
    } else {
        position->x = Math16_Wrap((int32_t)position->x - dx);
        if (position->x < 16) {
            position->x = 16;
        }
    }
    position->y = Math16_Wrap((int32_t)position->y + dy);
    M_FlowerAdd(
        Math16_Wrap((int32_t)position->x + 12),
        Math16_Wrap((int32_t)position->y + 12));
}

static void M_Drop(BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    const int16_t frame = boss->phase_frame;
    if (frame < 10) {
        return;
    }
    if (frame == 10) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 142;
    } else if (frame == 50) {
        SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        boss->sprite = 143;
    } else if (frame > 50 && frame <= 90) {
        ENTITY_POINT *const position = M_Position(boss);
        position->y = Math16_Wrap((int32_t)position->y + 6);
    } else if (frame > 90) {
        boss->sprite = 141;
        boss->phase_frame = 0;
    }
}

static void M_Dash(BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    MEIRA *const m = &m_Meira;
    ENTITY_POINT *const position = M_Position(boss);
    const int16_t frame = boss->phase_frame;
    const int16_t top = position->y;
    if (frame < 20) {
        return;
    }
    if (frame == 20) {
        m->direction =
            Math16_Wrap((int32_t)position->x + 16) < g_Player->current.x;
        boss->sprite = m->direction + 144;
        m->flower_frames = 16;
        m->flower_sprite = 121 - m->direction;
        m->flower_group = BULLET_AIMED;
        m->flower_speed = 64;
    } else if (frame <= 32) {
        M_Move(boss, -8, -8);
        m->flower_frames = Math16_Wrap((int32_t)m->flower_frames + 2);
    } else if (frame < 56) {
        boss->sprite = 141;
    } else if (frame == 56) {
        boss->sprite = 145 - m->direction;
        m->flower_sprite = m->direction + 120;
        m->flower_frames = 16;
    } else if (frame == 57) {
        M_Move(boss, 8, -8);
        m->flower_frames = Math16_Wrap((int32_t)m->flower_frames + 2);
        if (top > 48) {
            boss->phase_frame = 56;
        }
    } else if (frame < 80) {
        boss->sprite = 141;
    } else if (frame == 80) {
        boss->sprite = 149 - m->direction;
        m->flower_sprite = 121 - m->direction;
        m->flower_frames = 16;
    } else if (position->x > 32 && position->x < 352) {
        M_Move(boss, 8, 8);
        m->flower_frames = Math16_Wrap((int32_t)m->flower_frames + 2);
    } else if (top > 48) {
        position->y = Math16_Wrap((int32_t)position->y - 2);
        boss->sprite = 141;
    } else {
        boss->phase_frame = 0;
    }
}

static void M_Cross(BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    MEIRA *const m = &m_Meira;
    ENTITY_POINT *const position = M_Position(boss);
    const int16_t frame = boss->phase_frame;
    if (frame < 50) {
        return;
    }
    if (frame == 50) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 142;
        m->flower_group = m->spread_pattern;
    } else if (frame == 99) {
        SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        boss->sprite = 149 - m->direction;
        m->flower_sprite = m->direction + 120;
        m->flower_frames = 80;
    } else if (frame >= 100 && frame < 120) {
        M_Move(boss, -8, 8);
        m->flower_frames = Math16_Wrap((int32_t)m->flower_frames - 1);
    } else if (frame >= 120 && frame < 136) {
        boss->sprite = m->direction + 144;
        m->flower_sprite = 121 - m->direction;
        m->flower_frames = Math16_Wrap((int32_t)m->flower_frames - 1);
        if (frame == 135) {
            SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        }
    } else if (frame >= 136 && frame < 156) {
        M_Move(boss, -8, -8);
        m->flower_frames = Math16_Wrap((int32_t)m->flower_frames - 1);
    } else if (frame >= 156) {
        if (position->x != 192) {
            boss->sprite = 141;
            position->x = Math16_Wrap(
                (int32_t)position->x + (m->direction == 0 ? 2 : -2));
        } else {
            boss->phase_frame = 0;
        }
    }
}

static void M_Fan(BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    const int16_t frame = boss->phase_frame;
    MEIRA *const m = &m_Meira;
    const ENTITY_POINT position = *M_Position(boss);
    if (frame < 50) {
        return;
    }
    if (frame == 50) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 142;
    } else if (frame == 99) {
        SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        boss->sprite = 143;
        m->speed[0] = 32;
    } else if (frame >= 100 && frame < 116) {
        if ((frame & 1) != 0) {
            Bullets_AddPellets(
                g_Stage->scroll.back, Math16_Wrap((int32_t)position.x + 24),
                Math16_Wrap((int32_t)position.y + 32), 64, m->fan_pattern,
                m->speed[0]);
            m->speed[0] = Math16_Wrap((int32_t)m->speed[0] + 11);
        }
    } else if (frame >= 116) {
        boss->sprite = 141;
        boss->phase_frame = 0;
    }
}

static void M_Rapid(BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    const int16_t frame = boss->phase_frame;
    MEIRA *const m = &m_Meira;
    ENTITY_POINT *const position = M_Position(boss);
    if (frame < 50) {
        return;
    }
    if (frame == 50) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 142;
        m->flower_group = BULLET_RANDOM_ONE;
    } else if (frame == 99) {
        SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        boss->sprite = 143;
        m->speed[1] = 32;
    } else if (frame >= 100 && frame < 120) {
        if ((frame & 1) != 0) {
            Bullets_AddPellets(
                g_Stage->scroll.back, Math16_Wrap((int32_t)position->x + 24),
                Math16_Wrap((int32_t)position->y + 32), 0, m->burst_pattern,
                m->speed[1]);
            m->speed[1] = Math16_Wrap((int32_t)m->speed[1] + 11);
        }
        if (position->x != 192) {
            position->x = Math16_Wrap(
                (int32_t)position->x + (position->x < 192 ? 8 : -8));
        }
    } else if (frame >= 120) {
        boss->sprite = 141;
        boss->phase_frame = 0;
    }
}

static void M_Square(BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    MEIRA *const m = &m_Meira;
    ENTITY_POINT *const position = M_Position(boss);
    const int16_t frame = boss->phase_frame;
    if (frame < 50) {
        return;
    }
    if (frame == 50) {
        m->direction =
            Math16_Wrap((int32_t)position->x + 16) < g_Player->current.x;
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 142;
        m->flower_group = BULLET_RANDOM_ONE;
        m->flower_speed = 48;
    } else if (frame == 99) {
        SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        boss->sprite = m->direction + 148;
        m->flower_sprite = m->direction + 120;
        m->flower_frames = 20;
    } else if (frame >= 100 && frame < 116) {
        M_Move(boss, -8, 8);
        m->flower_frames = Math16_Wrap((int32_t)m->flower_frames + 2);
    } else if (frame >= 116 && frame < 136) {
        boss->sprite = 149 - m->direction;
        m->flower_sprite = 121 - m->direction;
        m->flower_frames = 20;
        if (frame == 135) {
            SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        }
    } else if (frame >= 136 && frame < 152) {
        M_Move(boss, 8, 8);
        m->flower_frames = Math16_Wrap((int32_t)m->flower_frames + 2);
    } else if (frame >= 152 && frame < 172) {
        boss->sprite = 145 - m->direction;
        m->flower_sprite = m->direction + 120;
        m->flower_frames = 20;
        if (frame == 171) {
            SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        }
    } else if (frame >= 172 && frame < 188) {
        M_Move(boss, 8, -8);
        m->flower_frames = Math16_Wrap((int32_t)m->flower_frames + 2);
    } else if (frame >= 188 && frame < 208) {
        boss->sprite = m->direction + 144;
        m->flower_sprite = 121 - m->direction;
        m->flower_frames = 20;
        if (frame == 207) {
            SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
        }
    } else if (frame >= 208 && frame < 224) {
        M_Move(boss, -8, -8);
        m->flower_frames = Math16_Wrap((int32_t)m->flower_frames + 2);
    } else if (frame >= 224) {
        boss->sprite = 141;
        boss->phase_frame = 0;
    }
}

static void M_MeiraBurst(BOSS_STATE *const boss)
{
    const ENTITY_POINT position = *M_Position(boss);
    SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
    boss->sprite = 143;
    Bullets_AddPellets(
        g_Stage->scroll.back, Math16_Wrap((int32_t)position.x + 24),
        Math16_Wrap((int32_t)position.y + 32), 0, m_Meira.burst_pattern, 74);
}

static void M_Triple(BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    const int16_t frame = boss->phase_frame;
    if (frame < 50) {
        return;
    }
    if (frame == 50) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 142;
        m_Meira.flower_group = BULLET_RANDOM_ONE;
    } else if (frame == 99 || frame == 108 || frame == 116) {
        M_MeiraBurst(boss);
    } else if (frame >= 100 && frame < 116) {
        boss->sprite = 142;
    } else if (frame > 116) {
        boss->sprite = 141;
        boss->phase_frame = 0;
    }
}

static void M_OpenBilliards(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    const int16_t frame = boss->phase_frame;
    if (frame < 50) {
        return;
    }
    if (frame == 50) {
        SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
        boss->sprite = 142;
        Bullets_SetTurnLimit(5);
    } else if (frame >= 99) {
        boss->sprite = 146;
        M_Billiards(boss, 3);
        boss->phase_frame = 0;
        m_Meira.trail_active = true;
    }
}

static void M_Flight(BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    MEIRA *const m = &m_Meira;
    ENTITY_POINT *const position = M_Position(boss);
    const int16_t frame = boss->phase_frame;
    if (frame < 50) {
        return;
    }
    ENTITY_POINT *const trail = m->trail[g_Stage->scroll.back];
    if ((frame & 7) == 0) {
        trail[0] = trail[1];
        trail[1] = trail[2];
        trail[2] = *position;
    }
    if (frame == 50) {
        for (size_t i = 0; i < MEIRA_TRAIL_COUNT; i++) {
            trail[i] = *position;
        }
        m->target.x = Random_RingU16() % 320 + 32;
        m->target.y = Random_RingU8() + 16;
        m->origin = *position;
        m->step = 1;
    } else if (frame <= 114) {
        const int16_t dx = Math16_Wrap((int32_t)m->target.x - m->origin.x);
        const int16_t dy = Math16_Wrap((int32_t)m->target.y - m->origin.y);
        position->x = Math16_Wrap(
            (int32_t)Math16_Wrap((int32_t)dx * m->step) / 64 + m->origin.x);
        position->y = Math16_Wrap(
            (int32_t)Math16_Wrap((int32_t)dy * m->step) / 64 + m->origin.y);
        m->step = Math16_Wrap((int32_t)m->step + 1);
    } else if (frame < 164) {
        return;
    } else {
        boss->sprite = 146;
        M_Billiards(boss, 2);
        boss->phase_frame = 0;
    }
    for (size_t i = 0; i < MEIRA_TRAIL_COUNT; i++) {
        if (trail[i].x != position->x && trail[i].y != position->y) {
            Draw_SpriteRollPlane(
                M_Pattern(patterns, 147), trail[i].x, M_Top(trail[i].y), 0,
                0xffc1);
        }
    }
}

static void M_Attack(BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    static const MEIRA_ATTACK opening[] = {
        M_Drop,
        M_Dash,
        M_Cross,
        M_Fan,
    };
    static const MEIRA_ATTACK pressure[] = {
        M_Rapid,
        M_Square,
        M_Triple,
    };
    static const MEIRA_ATTACK billiards[] = {
        M_OpenBilliards,
        M_Flight,
    };
    MEIRA *const m = &m_Meira;
    if (m->phase == MEIRA_OPENING) {
        if (m->pattern < ARRAY_SIZE(opening)) {
            opening[m->pattern](boss, patterns);
        }
        if (boss->phase_frame == 0) {
            m->pattern++;
            if (m->pattern >= ARRAY_SIZE(opening)) {
                m->pattern = 0;
            }
            if (boss->damage > 700 && m->pattern != 1) {
                m->phase = MEIRA_PRESSURE;
                m->pattern = 0;
            }
        }
    } else if (m->phase == MEIRA_PRESSURE) {
        if (m->pattern < ARRAY_SIZE(pressure)) {
            pressure[m->pattern](boss, patterns);
        }
        if (boss->phase_frame == 0) {
            m->pattern++;
            if (m->pattern >= ARRAY_SIZE(pressure)) {
                m->pattern = 0;
            }
            if (boss->damage > 1500) {
                m->phase = MEIRA_BILLIARDS;
                m->pattern = 0;
            }
        }
    } else if (m->phase == MEIRA_BILLIARDS) {
        if (m->pattern < ARRAY_SIZE(billiards)) {
            billiards[m->pattern](boss, patterns);
        }
        if (boss->phase_frame == 0) {
            m->pattern = 1;
        }
    }
}

static void M_HitAndDraw(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    const ENTITY_POINT position = *M_Position(boss);
    const ENTITY_POINT player = g_Player->current;
    if (position.x <= player.x
        && Math16_Wrap((int32_t)position.x + 32) > player.x
        && Math16_Wrap((int32_t)position.y - 16) < player.y
        && Math16_Wrap((int32_t)position.y + 32) > player.y) {
        Player_Hit();
    }
    const int16_t top = M_Top(position.y);
    if (Boss_Hit(
            &boss->damage, Math16_Wrap((int32_t)position.x + 8), position.y, 48,
            48, 1, 1)
        != 0) {
        SoundEffects_Play(SOUND_EFFECT_HIT);
        Draw_SpriteRollPlane(
            M_Pattern(patterns, boss->sprite), position.x, top, 0, 0xffc0);
        if (boss->damage >= 2400) {
            boss->defeated = 1;
            Score_AddPoints(30000);
            Player_BeginInvincibility(BOSS_DEFEAT_INVINCIBILITY_FRAMES);
        }
    } else {
        Draw_SpriteRoll(M_Pattern(patterns, boss->sprite), position.x, top);
    }
}

static bool M_Defeat(BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    MEIRA *const m = &m_Meira;
    const ENTITY_POINT position = *M_Position(boss);
    const int16_t x = Math16_Wrap((int32_t)position.x + 24);
    const int16_t y = Math16_Wrap((int32_t)position.y + 24);
    BossExplosion_Draw(patterns, boss->angle, x, y, m->kill_frame);
    BossExplosion_Draw(
        patterns, boss->angle, x, y, Math16_Wrap((int32_t)m->kill_frame - 24));
    Sparks_Add(
        g_Stage->scroll.back, Math16_Wrap((int32_t)position.x + 48),
        (uint16_t)(position.y + 48), 60, 1, false);
    const int16_t image = Math16_Wrap((int32_t)m->kill_frame - 32) / 10 + 10;
    m->kill_frame = Math16_Wrap((int32_t)m->kill_frame + 1);
    if (m->kill_frame >= 96) {
        m->kill_frame = 0;
        return true;
    }
    if (boss->phase_frame < 32) {
        Draw_SpriteRoll(
            M_Pattern(patterns, boss->sprite), position.x, M_Top(position.y));
    } else {
        Draw_SpriteZoom(
            M_Pattern(patterns, image), position.x, M_Top(position.y), 2);
    }
    return false;
}

static void M_Init(void)
{
    memset(&m_Meira, 0, sizeof(m_Meira));
    memset(&m_Midboss, 0, sizeof(m_Midboss));
}

bool Meira_MidbossInvalidate(BOSS_STATE *const boss)
{
    const uint8_t back = g_Stage->scroll.back;
    const uint8_t front = g_Stage->scroll.front;
    const ENTITY_POINT position = boss->position[back];
    Background_Mark(g_Stage->scroll.line, position.x, position.y, 64, 64);
    boss->position[back] = boss->position[front];
    return m_Midboss.active;
}

void Meira_MidbossUpdate(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    ENTITY_POINT *const position = &boss->position[g_Stage->scroll.back];
    Shots_SetTarget(
        (ENTITY_POINT) { 216, Math16_Wrap((int32_t)position->y + 24) });
    boss->phase_frame = Math16_Wrap((int32_t)boss->phase_frame + 1);
    if (boss->phase_frame == 1) {
        boss->position[0] = boss->position[1] = (ENTITY_POINT) { 192, -16 };
        boss->damage = 0;
        m_Midboss.active = true;
        StageEffects_StartLightning();
        return;
    }
    const int16_t image = ((g_Stage->scroll.line >> 1) & 3) + 137;
    int16_t top;
    if (boss->phase_frame < 320) {
        if ((boss->phase_frame & 3) == 0) {
            position->y = Math16_Wrap((int32_t)position->y + 1);
        }
        top = M_RolledTop(position->y);
        if (Boss_Hit(&boss->damage, position->x, position->y, 64, 58, 0, 1)
            != 0) {
            M_Flash(boss, patterns, top);
            boss->damage = Math16_Wrap((int32_t)boss->damage + 1);
            return;
        }
    } else {
        if (boss->defeated != 0) {
            M_MidbossDefeat(boss, patterns);
            return;
        }
        M_MidbossFire(boss);
        top = M_RolledTop(position->y);
        if (Boss_Hit(&boss->damage, position->x, position->y, 64, 58, 1, 1)
            != 0) {
            if (boss->damage <= 380 || top >= 304) {
                M_Flash(boss, patterns, top);
            } else {
                SoundEffects_Play(SOUND_EFFECT_BOSS_HEAVY_HIT);
                boss->defeated = 1;
                Score_AddPoints(20000);
            }
            return;
        }
        if (boss->phase_frame > 1600) {
            boss->defeated = 1;
        }
    }
    Draw_SpriteRoll(M_Pattern(patterns, image), position->x, top);
}

void Meira_Place(BOSS_STATE *const boss)
{
    boss->position[0] = boss->position[1] = (ENTITY_POINT) { 192, 48 };
}

static void M_DrawEntrance(
    const BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    const ENTITY_POINT position = boss->position[g_Stage->scroll.back];
    Draw_SpriteRoll(M_Pattern(patterns, boss->sprite), position.x, M_Top(48));
}

void Meira_Begin(BOSS_STATE *const boss, const uint8_t rank)
{
    Boss_ResetDamage(&boss->damage, 2400);
    boss->defeated = 0;
    Bullets_SetTurnLimit(2);
    boss->angle = 0;
    m_Meira.phase = MEIRA_OPENING;
    m_Meira.pattern = 0;
    m_Meira.trail_active = false;
    m_Meira.spread_pattern = BULLET_RANDOM_ONE;
    const bool easy = rank == 0;
    m_Meira.fan_pattern = easy ? BULLET_FAN3_WIDE : BULLET_FAN5_WIDE;
    m_Meira.burst_pattern =
        easy ? BULLET_FAN3_MEDIUM_AIMED : BULLET_FAN5_MEDIUM_AIMED;
}

void Meira_Invalidate(BOSS_STATE *const boss)
{
    const uint8_t back = g_Stage->scroll.back;
    const uint8_t front = g_Stage->scroll.front;
    const ENTITY_POINT position = boss->position[back];
    Background_Mark(g_Stage->scroll.line, position.x, position.y, 64, 64);
    if (m_Meira.trail_active) {
        for (size_t i = 0; i < MEIRA_TRAIL_COUNT; i++) {
            const ENTITY_POINT trail = m_Meira.trail[back][i];
            if (trail.x != position.x && trail.y != position.y) {
                Background_Mark(g_Stage->scroll.line, trail.x, trail.y, 64, 64);
            }
            m_Meira.trail[back][i] = m_Meira.trail[front][i];
        }
    }
    boss->position[back] = boss->position[front];
    for (size_t i = 0; i < MEIRA_FLOWER_COUNT; i++) {
        MEIRA_FLOWER *const p = &m_Meira.flowers[i];
        if (p->flag != ENTITY_FREE) {
            Background_Mark(
                g_Stage->scroll.line, p->position.x, p->position.y, 32, 32);
            if (p->flag == ENTITY_REMOVE) {
                p->flag = ENTITY_FREE;
            }
        }
    }
}

BOSS_RESULT Meira_Update(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns,
    const uint8_t rank)
{
    boss->phase_frame = Math16_Wrap((int32_t)boss->phase_frame + 1);
    const ENTITY_POINT position = *M_Position(boss);
    Shots_SetTarget((ENTITY_POINT) { Math16_Wrap((int32_t)position.x + 40),
                                     Math16_Wrap((int32_t)position.y + 40) });
    if (boss->defeated != 0) {
        if (M_Defeat(boss, patterns)) {
            return BOSS_FINISHED;
        }
    } else {
        M_Attack(boss, patterns);
        M_HitAndDraw(boss, patterns);
    }
    M_Flowers(patterns, rank);
    return BOSS_RUNNING;
}

static void M_Prepare(BOSS_STATE *const boss)
{
    boss->sprite = 141;
}

static void M_Begin(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const TH02_SESSION *const session)
{
    if (assets->stages[session->stage].boxes == nullptr) {
        Meira_Place(p_BossState);
    }
    Meira_Begin(p_BossState, (uint8_t)session->rank);
}

static void M_Invalidate(const bool reduce_effects)
{
    Meira_Invalidate(p_BossState);
}

static BOSS_RESULT M_Update(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, TH02_SESSION *const session,
    const TH02_INPUT *const input)
{
    return Meira_Update(p_BossState, patterns, (uint8_t)session->rank);
}

static bool M_MidbossInvalidate(void)
{
    return Meira_MidbossInvalidate(p_BossState);
}

static void M_MidbossUpdate(
    const PATTERN_BANK *const patterns, const uint8_t rank)
{
    Meira_MidbossUpdate(p_BossState, patterns);
}

const BOSS_STRATEGY g_BossMeira = {
    .init = M_Init,
    .begin = M_Begin,
    .invalidate = M_Invalidate,
    .update = M_Update,
    .post_dialogue = true,
    .midboss_invalidate = M_MidbossInvalidate,
    .midboss_update = M_MidbossUpdate,
    .talk = { .prepare = M_Prepare,
              .place = Meira_Place,
              .draw_scene = M_DrawEntrance },
};
