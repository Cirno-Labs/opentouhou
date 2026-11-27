#include "th02/background.h"
#include "th02/boss.h"
#include "th02/boss/explosion.h"
#include "th02/boss/private.h"
#include "th02/boss/strategy.h"
#include "th02/bullets.h"
#include "th02/pillars.h"
#include "th02/player.h"
#include "th02/score.h"
#include "th02/shots.h"
#include "th02/sound_effects.h"
#include "th02/sparks.h"
#include "th02/stage.h"
#include "th02/stage_effects.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

typedef enum {
    RIKA_SWEEP,
    RIKA_CENTER,
    RIKA_HOLD,
} RIKA_PHASE;

typedef struct {
    ENTITY_POINT current;
    RIKA_PHASE phase;
    int16_t kill_frame;
    uint16_t frame;
    uint8_t angle;
    uint8_t first_pattern;
    uint8_t second_pattern;
    uint8_t mirrored_pattern;
    uint8_t fire_mask;
} RIKA;

typedef struct {
    int16_t kill_frame;
    int16_t sprite;
    bool active;
} RIKA_MIDBOSS;

static RIKA m_Rika;
static RIKA_MIDBOSS m_Midboss;

static const PATTERN *M_Pattern(
    const PATTERN_BANK *const patterns, const int16_t id)
{
    ASSERT(id >= 0 && id < PATTERN_LIMIT);
    const PATTERN *const pattern = &patterns->entries[id];
    ASSERT(pattern->data != nullptr);
    return pattern;
}

static void M_Put(
    const BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    Draw_SpriteRoll(
        M_Pattern(patterns, boss->sprite), m_Rika.current.x, m_Rika.current.y);
}

static void M_Move(BOSS_STATE *const boss, const uint8_t rank)
{
    RIKA *const rika = &m_Rika;
    int16_t *const left = &boss->position[g_Stage->scroll.back].x;
    if (rika->phase == RIKA_SWEEP) {
        if (boss->phase_frame < 16) {
            if ((boss->phase_frame & 3) == 0) {
                const uint8_t angle =
                    rank >= 2 ? (uint8_t)boss->phase_frame : 0;
                Bullets_AddPellets(
                    g_Stage->scroll.back, 228, 112, angle, BULLET_RING16,
                    Math16_Wrap((int32_t)boss->phase_frame * 4 + 40));
            }
        } else if (boss->phase_frame < 116) {
            *left = Math16_Wrap((int32_t)*left - 1);
        } else if (boss->phase_frame < 316) {
            *left = Math16_Wrap((int32_t)*left + 1);
        } else {
            if (boss->phase_frame >= 416) {
                boss->phase_frame = 16;
            }
            *left = Math16_Wrap((int32_t)*left - 1);
        }
    } else if (rika->phase == RIKA_CENTER) {
        if (*left < 192) {
            *left = Math16_Wrap((int32_t)*left + 1);
        } else if (boss->phase_frame > 192) {
            *left = Math16_Wrap((int32_t)*left - 1);
        } else {
            rika->phase = RIKA_HOLD;
        }
    } else if (rika->phase == RIKA_HOLD) {
        boss->phase_frame = 0;
    }
}

static bool M_Hit(BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    if (Boss_Hit(&boss->damage, m_Rika.current.x, 48, 64, 96, 1, 1) == 0) {
        return false;
    }
    if (boss->damage <= 2260) {
        SoundEffects_Play(SOUND_EFFECT_HIT);
        Draw_SpriteRollPlane(
            M_Pattern(patterns, boss->sprite), m_Rika.current.x,
            m_Rika.current.y, 0, 0xffc0);
    } else {
        boss->defeated = 1;
        Score_AddPoints(20000);
        Player_BeginInvincibility(BOSS_DEFEAT_INVINCIBILITY_FRAMES);
    }
    return true;
}

static void M_Fire(BOSS_STATE *const boss)
{
    RIKA *const rika = &m_Rika;
    int16_t left = Math16_Wrap((int32_t)rika->current.x + 28);
    if (boss->damage < 700) {
        rika->phase = RIKA_SWEEP;
        if ((boss->phase_frame & 63) == 0) {
            SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
            Bullets_AddPellets(
                g_Stage->scroll.back, left, 112, 0, rika->first_pattern, 50);
            Bullets_AddPellets(
                g_Stage->scroll.back, left, 112, 64, rika->second_pattern, 30);
        }
    } else if (boss->damage < 1400) {
        if ((g_Stage->world.frame & 31) == 0) {
            rika->angle = (uint8_t)(rika->angle + 24) % 128;
            left = Math16_Wrap((int32_t)rika->current.x + 8);
            SoundEffects_Play(SOUND_EFFECT_PATTERN_FIRE);
            Bullets_AddPellets(
                g_Stage->scroll.back, left, 112, rika->angle, BULLET_SINGLE,
                54);
            Bullets_AddPellets(
                g_Stage->scroll.back, left, 112, rika->angle,
                rika->mirrored_pattern, 44);
            left = Math16_Wrap((int32_t)left + 44);
            const uint8_t angle = (uint8_t)(128 - rika->angle);
            Bullets_AddPellets(
                g_Stage->scroll.back, left, 112, angle, BULLET_SINGLE, 54);
            Bullets_AddPellets(
                g_Stage->scroll.back, left, 112, angle, rika->mirrored_pattern,
                44);
        }
    } else {
        if (rika->phase == RIKA_SWEEP) {
            rika->phase = RIKA_CENTER;
            rika->frame = 0;
            rika->angle = 0;
        }
        rika->frame++;
        if (rika->frame < 100) {
            boss->sprite = (rika->frame & 7) < 4 ? 150 : 151;
            if ((rika->frame & 31) == 0) {
                SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
            }
        } else if (rika->frame == 100) {
            Pillars_SetSpawnInterval(64);
            Pillars_Add(
                Math16_Wrap((int32_t)rika->current.x + 2), 112, 160, 111);
            Pillars_Add(
                Math16_Wrap((int32_t)rika->current.x + 46), 112, 160, 111);
        } else if (rika->frame > 164 && rika->frame < 280) {
            if (rika->frame < 250) {
                boss->sprite = (rika->frame & 7) < 4 ? 150 : 151;
            } else if (rika->frame == 260) {
                boss->sprite = 152;
            } else if (rika->frame == 270) {
                boss->sprite = 153;
            }
            if ((rika->frame & 15) == 0) {
                SoundEffects_Play(SOUND_EFFECT_PILLAR);
                const uint8_t pattern = (rika->frame & 16) == 0
                    ? BULLET_FAN4_WIDE
                    : BULLET_FAN4_MEDIUM;
                Bullets_AddPellets(
                    g_Stage->scroll.back, left, 112, 64, pattern, 58);
            }
        } else if (rika->frame >= 280 && (rika->frame & rika->fire_mask) == 0) {
            Bullets_AddSprites(
                g_Stage->scroll.back, left, 72, 1, BULLET_RANDOM_SPEED, 80, 44);
        }
    }
}

static bool M_Defeat(BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    RIKA *const rika = &m_Rika;
    Sparks_Add(
        g_Stage->scroll.back, Math16_Wrap((int32_t)rika->current.x + 32), 128,
        50, 2, false);
    const int16_t image = Math16_Wrap((int32_t)rika->kill_frame - 32) / 8 + 10;
    rika->kill_frame = Math16_Wrap((int32_t)rika->kill_frame + 1);
    if (rika->kill_frame >= 80) {
        rika->kill_frame = 0;
        return true;
    }
    if (boss->phase_frame < 32) {
        M_Put(boss, patterns);
    } else {
        Draw_SpriteZoom(
            M_Pattern(patterns, image), rika->current.x,
            Math16_Wrap((int32_t)rika->current.y + 16), 2);
    }
    BossExplosion_Draw(
        patterns, boss->angle, Math16_Wrap((int32_t)rika->current.x + 24), 112,
        rika->kill_frame);
    return false;
}

static void M_MidbossDefeat(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    RIKA_MIDBOSS *const midboss = &m_Midboss;
    ENTITY_POINT *const position = &boss->position[g_Stage->scroll.back];
    const int16_t image = 10 + (midboss->kill_frame >> 3);
    midboss->kill_frame = Math16_Wrap((int32_t)midboss->kill_frame + 1);
    const int16_t left = Math16_Wrap((int32_t)position->x + 32);
    const uint16_t top = (uint16_t)(position->y + 48);
    Sparks_Add(g_Stage->scroll.back, left, top, 60, 1, false);
    if (midboss->kill_frame >= 64) {
        Sparks_Add(g_Stage->scroll.back, left, top, 128, 48, true);
        midboss->kill_frame = 0;
        boss->phase_frame = 0;
        Shots_ResetTarget();
        midboss->active = false;
        return;
    }
    if ((midboss->kill_frame & 3) == 0) {
        position->y = Math16_Wrap((int32_t)position->y + 1);
    }
    int16_t rolled = Math16_Wrap((int32_t)position->y + g_Stage->scroll.line);
    if (rolled >= 400) {
        rolled -= 400;
    }
    Draw_SpriteZoom(
        M_Pattern(patterns, image), Math16_Wrap((int32_t)position->x - 16),
        rolled, 3);
}

static void M_MidbossFire(const BOSS_STATE *const boss)
{
    const int16_t frame = boss->phase_frame & 127;
    const ENTITY_POINT position = boss->position[g_Stage->scroll.back];
    const int16_t left = Math16_Wrap((int32_t)position.x + 28);
    const int16_t top = Math16_Wrap((int32_t)position.y + 88);
    if (frame == 0 || frame == 96) {
        Bullets_AddPellets(
            g_Stage->scroll.back, left, top, 0, BULLET_FAN4_WIDE_AIMED, 50);
    } else if (frame == 48) {
        Bullets_AddPellets(
            g_Stage->scroll.back, left, top, 0, BULLET_FAN3_MEDIUM_AIMED, 30);
    }
}

static void Rika_Init(void)
{
    memset(&m_Rika, 0, sizeof(m_Rika));
    memset(&m_Midboss, 0, sizeof(m_Midboss));
}

static bool Rika_MidbossInvalidate(BOSS_STATE *const boss)
{
    const uint8_t back = g_Stage->scroll.back;
    const uint8_t front = g_Stage->scroll.front;
    const ENTITY_POINT position = boss->position[back];
    const bool defeated = boss->defeated != 0;
    Background_Mark(
        g_Stage->scroll.line,
        Math16_Wrap((int32_t)position.x - (defeated ? 16 : 0)), position.y,
        defeated ? 96 : 64, 96);
    boss->position[back] = boss->position[front];
    return m_Midboss.active;
}

static void Rika_MidbossUpdate(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns)
{
    RIKA_MIDBOSS *const midboss = &m_Midboss;
    ENTITY_POINT *const position = &boss->position[g_Stage->scroll.back];
    Shots_SetTarget((ENTITY_POINT) { 216, 64 });
    boss->phase_frame = Math16_Wrap((int32_t)boss->phase_frame + 1);
    if (boss->phase_frame == 1) {
        boss->position[0] = boss->position[1] = (ENTITY_POINT) { 192, -16 };
        boss->damage = 0;
        midboss->sprite = 148;
        midboss->active = true;
        StageEffects_StartStorm();
        return;
    }
    int16_t rolled;
    if (boss->phase_frame < 18) {
        position->y = Math16_Wrap((int32_t)position->y + 2);
        rolled = Math16_Wrap((int32_t)position->y + g_Stage->scroll.line);
        if (rolled < 0) {
            rolled = Math16_Wrap((int32_t)rolled + 400);
        } else if (rolled >= 400) {
            rolled -= 400;
        }
    } else {
        midboss->sprite = (g_Stage->scroll.line & 1) != 0 ? 149 : 148;
        if (boss->defeated != 0) {
            M_MidbossDefeat(boss, patterns);
            return;
        }
        M_MidbossFire(boss);
        if ((g_Stage->scroll.line & 15) == 0) {
            int16_t top = Math16_Wrap((int32_t)48 + g_Stage->scroll.line);
            if (top >= 400) {
                top -= 400;
            }
            for (int i = 0; i < 4; i++) {
                Background_SetTile(
                    g_Stage->scroll.front, g_Stage->scroll.back,
                    Math16_Wrap((int32_t)position->x + i * 16), top,
                    (uint8_t)(60 + i));
            }
        }
        rolled = Math16_Wrap((int32_t)position->y + g_Stage->scroll.line);
        if (rolled >= 400) {
            rolled -= 400;
        }
        if (Boss_Hit(&boss->damage, position->x, position->y, 64, 96, 1, 1)
            != 0) {
            if (boss->damage <= 300 || rolled >= 304) {
                SoundEffects_Play(SOUND_EFFECT_HIT);
                Draw_SpriteRollPlane(
                    M_Pattern(patterns, 148), position->x, rolled, 0, 0xffc0);
            } else {
                SoundEffects_Play(SOUND_EFFECT_BOSS_HEAVY_HIT);
                boss->defeated = 1;
                Score_AddPoints(10000);
            }
            return;
        }
        if (Math16_Wrap(g_Stage->scroll.rows) >= 184 && position->y <= 304) {
            SoundEffects_Play(SOUND_EFFECT_BOSS_HEAVY_HIT);
            boss->defeated = 1;
            return;
        }
    }
    Draw_SpriteRoll(M_Pattern(patterns, midboss->sprite), position->x, rolled);
}

static void Rika_Begin(BOSS_STATE *const boss, const uint8_t rank)
{
    boss->angle = 32;
    boss->position[0].x = 192;
    boss->position[1].x = 192;
    Boss_ResetDamage(&boss->damage, 2261);
    boss->defeated = 0;
    boss->sprite = 150;
    Stage_RestartFrames();
    m_Rika.phase = RIKA_SWEEP;
    Pillars_Enable();
    m_Rika.current.y = Math16_Wrap((int32_t)48 + g_Stage->scroll.line);
    if (m_Rika.current.y >= 400) {
        m_Rika.current.y -= 400;
    }
    const bool easy = rank == 0;
    m_Rika.first_pattern = easy ? BULLET_AIMED : BULLET_FAN3_MEDIUM_AIMED;
    m_Rika.second_pattern = BULLET_FAN5_WIDE;
    m_Rika.mirrored_pattern = easy ? BULLET_SINGLE : BULLET_FAN2_MEDIUM;
    m_Rika.fire_mask = easy ? 7 : 3;
}

static void Rika_Invalidate(BOSS_STATE *const boss)
{
    const uint8_t back = g_Stage->scroll.back;
    const uint8_t front = g_Stage->scroll.front;
    Background_Mark(g_Stage->scroll.line, boss->position[back].x, 48, 64, 96);
    boss->position[back].x = boss->position[front].x;
    if (m_Rika.phase == RIKA_SWEEP && boss->phase_frame < 3) {
        Background_Mark(g_Stage->scroll.line, 208, 16, 32, 32);
    }
}

static BOSS_RESULT Rika_Update(
    BOSS_STATE *const boss, const PATTERN_BANK *const patterns,
    const uint8_t rank)
{
    Shots_SetTarget(
        (ENTITY_POINT) { Math16_Wrap((int32_t)m_Rika.current.x + 24), 48 });
    boss->phase_frame = Math16_Wrap((int32_t)boss->phase_frame + 1);
    if (boss->defeated == 0) {
        M_Move(boss, rank);
        m_Rika.current.x = boss->position[g_Stage->scroll.back].x;
        if (!M_Hit(boss, patterns)) {
            M_Put(boss, patterns);
        }
        M_Fire(boss);
    } else if (M_Defeat(boss, patterns)) {
        return BOSS_FINISHED;
    }
    return BOSS_RUNNING;
}

static void M_Begin(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const TH02_SESSION *const session)
{
    Rika_Begin(p_BossState, (uint8_t)session->rank);
}

static void M_Invalidate(const bool reduce_effects)
{
    Rika_Invalidate(p_BossState);
}

static BOSS_RESULT M_Update(
    PATTERN_BANK *const patterns, const BOSS_ASSETS *const assets,
    const BOSS_MUSIC *const music, TH02_SESSION *const session,
    const TH02_INPUT *const input)
{
    return Rika_Update(p_BossState, patterns, (uint8_t)session->rank);
}

static bool M_MidbossInvalidate(void)
{
    return Rika_MidbossInvalidate(p_BossState);
}

static void M_MidbossUpdate(
    const PATTERN_BANK *const patterns, const uint8_t rank)
{
    Rika_MidbossUpdate(p_BossState, patterns);
}

const BOSS_STRATEGY g_BossRika = {
    .init = Rika_Init,
    .begin = M_Begin,
    .invalidate = M_Invalidate,
    .update = M_Update,
    .post_dialogue = true,
    .midboss_invalidate = M_MidbossInvalidate,
    .midboss_update = M_MidbossUpdate,
    .talk = { .music_before = true },
};
