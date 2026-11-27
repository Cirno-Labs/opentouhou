#include "th02/background.h"
#include "th02/bomb.h"
#include "th02/bomb/private.h"
#include "th02/bullets.h"
#include "th02/draw.h"
#include "th02/grcg.h"
#include "th02/hud.h"
#include "th02/palette.h"
#include "th02/performance.h"
#include "th02/player.h"
#include "th02/random.h"
#include "th02/score.h"
#include "th02/sound_effects.h"
#include "th02/sparks.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

static BOMB m_Bomb;
const BOMB *const g_Bomb = &m_Bomb;

typedef struct {
    bool (*update)(
        PLAYFIELD_PAGES pages, const BOMB_ASSETS *assets, uint8_t *slowdown,
        bool reduce_effects, const uint8_t *transition_tail,
        bool fix_transition_overread);
    int16_t restore_frame;
} BOMB_STRATEGY;

static void M_Clear(const PLAYFIELD_PAGES pages)
{
    Bullets_RemoveAll(pages);
}

static void M_BombCircle(
    const BOMB_ASSETS *const assets, const int16_t scroll, const int16_t radius,
    const uint8_t first_angle, const bool erase)
{
    uint8_t angle = first_angle;
    for (size_t i = 0; i < 64; i++, angle = (uint8_t)(angle + 4)) {
        const int16_t left = Math16_PolarX(m_Bomb.center.x, radius, angle);
        const int16_t top = Math16_PolarY(m_Bomb.center.y, radius, angle);
        if (erase) {
            if (top > 8 && top < 384) {
                Background_Mark(scroll, left, top, 8, 8);
            }
        } else {
            BombRender_CirclePoint(assets, scroll, left, top);
        }
    }
}

static void M_BombParticles(
    const BOMB_ASSETS *const assets, const int16_t scroll_line)
{
    const size_t first = ((uint16_t)m_Bomb.frame & 15) * 2;
    for (size_t i = first; i < first + 2; i++) {
        m_Bomb.particles[i].x = Random_RingU16() % 384 + 32;
        m_Bomb.particles[i].y = Random_RingU16() % 368 + 16;
        m_Bomb.ages[i] = 1;
    }
    for (size_t i = 0; i < 32; i++) {
        if (m_Bomb.ages[i] == 0) {
            continue;
        }
        if (((uint16_t)m_Bomb.frame & 3) == (i & 3)) {
            m_Bomb.ages[i]++;
            if (m_Bomb.ages[i] >= 5) {
                m_Bomb.ages[i] = 0;
                continue;
            }
        }
        BombRender_Particle(
            assets, scroll_line, m_Bomb.particles[i].x, m_Bomb.particles[i].y,
            m_Bomb.ages[i] - 1);
    }
}

static void M_Transition(
    const BOMB_ASSETS *const assets, const int16_t scroll_line,
    const uint8_t *const transition_tail, const bool fix_transition_overread)
{
    const int16_t image = (m_Bomb.frame - 1) / 2;
    ASSERT(m_Bomb.frame >= 1 && image <= 15);
    const uint8_t *frame;
    if (image == 15) {
        if (fix_transition_overread) {
            return;
        }
        ASSERT(transition_tail != nullptr);
        frame = transition_tail;
    } else {
        frame = (const uint8_t *)assets->transition[image];
    }
    BombRender_Transition(scroll_line, frame);
}

static bool M_TypeA(
    const PLAYFIELD_PAGES pages, const BOMB_ASSETS *const assets,
    uint8_t *const slowdown, const bool reduce_effects,
    const uint8_t *const transition_tail, const bool fix_transition_overread)
{
    const int16_t frame = m_Bomb.frame;
    Grcg_SetColor(0);
    if (frame == 1) {
        memcpy(m_Bomb.color_zero, g_Palette->colors[0], 3);
        memcpy(m_Bomb.color_three, g_Palette->colors[3], 3);
        Palette_SetColor(3, 64, 16, 64);
        Palette_Show();
    }
    if (frame < 32) {
        Grcg_SetColor(3);
        M_Transition(
            assets, pages.scroll, transition_tail, fix_transition_overread);
    } else if (frame == 32) {
        Palette_SetColor(
            3, m_Bomb.color_three[0], m_Bomb.color_three[1],
            m_Bomb.color_three[2]);
        Palette_SetColor(0, 128, 0, 128);
        Palette_Show();
        m_Bomb.tile_modes[0] = g_Background->cache.mode;
        Background_SetMode(TILE_DISABLED);
        Draw_BoxFill(g_Clip, 32, 0, 415, 399);
    } else if (frame < 136) {
        if (frame > 64) {
            Grcg_SetColor(11);
        }
        Draw_BoxFill(g_Clip, 32, 0, 415, 399);
        Grcg_Off();
        if (frame <= 64) {
            const uint8_t value = (uint8_t)(256 - frame * 4);
            Palette_SetColor(0, value, 0, value);
            if (frame == 64) {
                Palette_SetTone(200);
                Palette_SetColor(0, 255, 255, 255);
            }
            Palette_Show();
        } else {
            Sparks_Add(pages.back, 212, 190, 50, 1, false);
            if (frame == 70) {
                Palette_SetTone(100);
            } else if (!reduce_effects || (frame & 1) != 0) {
                BombRender_Illustration(assets, pages.scroll, 112, 90);
            }
            if (frame >= 86) {
                const uint8_t tint = frame % 3 != 0 ? 255 : 160;
                Palette_SetColor(0, tint, tint, tint);
                Palette_SetTone(frame * 2 - 72);
                if (frame > 111) {
                    *slowdown = 2;
                }
            }
        }
        if (frame < 86) {
            Grcg_SetColor(15);
            if ((frame & 3) == 0) {
                SoundEffects_Play(SOUND_EFFECT_BOMB_1);
            }
            M_BombParticles(assets, pages.scroll);
        }
        if (frame == 86) {
            SoundEffects_Play(SOUND_EFFECT_BOMB_3);
        }
    } else if (frame == 136) {
        Palette_SetColor(
            0, m_Bomb.color_zero[0], m_Bomb.color_zero[1],
            m_Bomb.color_zero[2]);
        Palette_Show();
        SoundEffects_Play(SOUND_EFFECT_BOMB_2);
        Background_SetMode(m_Bomb.tile_modes[0]);
        *slowdown = 1;
        Palette_SetTone(200);
        M_Clear(pages);
    } else if (frame < 156) {
        Palette_SetTone(Math16_Wrap(880 - frame * 5));
    } else {
        Grcg_Off();
        return true;
    }
    Grcg_Off();
    return false;
}

static bool M_TypeB(
    const PLAYFIELD_PAGES pages, const BOMB_ASSETS *const assets,
    uint8_t *const slowdown, const bool reduce_effects,
    const uint8_t *const transition_tail, const bool fix_transition_overread)
{
    Grcg_SetColor(15);
    if (m_Bomb.frame < 32) {
        M_Transition(
            assets, pages.scroll, transition_tail, fix_transition_overread);
    } else if (m_Bomb.frame == 32) {
        m_Bomb.row = 0;
        m_Bomb.tile_modes[1] = g_Background->cache.mode;
        Background_SetMode(TILE_DISABLED);
        Draw_BoxFill(g_Clip, 32, 0, 415, 399);
    } else if (m_Bomb.frame < 132) {
        if (m_Bomb.frame > 32 && (m_Bomb.frame & 3) == 0) {
            if ((m_Bomb.frame & 7) == 4) {
                SoundEffects_Play(SOUND_EFFECT_BOMB_2);
            }
            BombRender_Text(assets, m_Bomb.row);
            m_Bomb.row++;
            if (m_Bomb.row >= 18) {
                Hud_WipePlayfield();
                m_Bomb.frame = 132;
            }
        }
        Draw_BoxFill(g_Clip, 32, 0, 415, 399);
        Grcg_SetColor(4);
        M_BombParticles(assets, pages.scroll);
    } else if (m_Bomb.frame < 164) {
        Draw_BoxFill(g_Clip, 32, 0, 415, 399);
        *slowdown = 2;
        if (!reduce_effects || (m_Bomb.frame & 1) != 0) {
            Grcg_Off();
            BombRender_Illustration(assets, pages.scroll, 32, 84);
        }
        if ((m_Bomb.frame & 3) == 0) {
            SoundEffects_Play(SOUND_EFFECT_BOMB_1);
        }
        const int16_t frame = m_Bomb.frame;
        const int16_t ring = frame <= 140 ? frame - 132
            : frame <= 148                ? frame - 140
            : frame <= 156                ? frame - 148
                                          : frame - 156;
        Grcg_SetColor(14);
        M_BombCircle(
            assets, pages.scroll, Math16_Wrap(ring * 32), (uint8_t)ring, false);
        Grcg_Off();
        Palette_SetTone(Math16_Wrap(596 - frame * 3));
    } else if (m_Bomb.frame == 164) {
        SoundEffects_Play(SOUND_EFFECT_BOMB_2);
        Background_SetMode(m_Bomb.tile_modes[1]);
        *slowdown = 1;
        Palette_SetTone(200);
        M_Clear(pages);
    } else if (m_Bomb.frame < 184) {
        Palette_SetTone(Math16_Wrap(1020 - m_Bomb.frame * 5));
    } else {
        Grcg_Off();
        return true;
    }
    Grcg_Off();
    return false;
}

static bool M_TypeC(
    const PLAYFIELD_PAGES pages, const BOMB_ASSETS *const assets,
    uint8_t *const slowdown, const bool reduce_effects,
    const uint8_t *const transition_tail, const bool fix_transition_overread)
{
    static const int8_t speeds[48] = {
        4, 3, 3, 2, 2, 1, 1, 0, 0, 2, 3, 2, 1, 0, 1, 2, 1, 0, 0, 0, 2, 3, 3, 2,
        3, 1, 2, 0, 0, 1, 1, 2, 2, 3, 1, 2, 3, 2, 1, 0, 1, 1, 2, 2, 3, 3, 4, 4,
    };
    const int16_t frame = m_Bomb.frame;
    Grcg_SetColor(2);
    if (frame < 32) {
        M_Transition(
            assets, pages.scroll, transition_tail, fix_transition_overread);
    } else if (frame == 32) {
        m_Bomb.tile_modes[2] = g_Background->cache.mode;
        Background_SetMode(TILE_DISABLED);
        Draw_BoxFill(g_Clip, 32, 0, 415, 399);
        for (size_t i = 0; i < 48; i++) {
            m_Bomb.columns[i].y = 20;
        }
        SoundEffects_Play(SOUND_EFFECT_BOMB_3);
    } else if (frame < 100) {
        Draw_BoxFill(g_Clip, 32, 0, 415, 399);
        if (frame >= 80) {
            Palette_SetTone(Math16_Wrap(frame * 5 - 300));
            *slowdown = 2;
        }
        Grcg_SetColor(14);
        M_BombParticles(assets, pages.scroll);
        Grcg_SetColor(1);
        for (size_t i = 0; i < 48; i++) {
            m_Bomb.columns[i].y =
                Math16_Wrap((int32_t)m_Bomb.columns[i].y + speeds[i]);
            BombRender_Smear(
                assets, pages.scroll, (int16_t)(i * 8 + 32),
                m_Bomb.columns[i].y);
        }
    } else if (frame < 112) {
        Palette_SetTone(100);
        if ((frame & 1) != 0) {
            Grcg_Off();
            BombRender_Illustration(assets, pages.scroll, 32, 16);
            SoundEffects_Play(SOUND_EFFECT_BOMB_2);
        } else {
            Grcg_SetColor(15);
            Draw_BoxFill(g_Clip, 32, 0, 415, 399);
        }
    } else if (frame == 112) {
        Background_SetMode(m_Bomb.tile_modes[2]);
        *slowdown = 1;
        Palette_SetTone(200);
        M_Clear(pages);
    } else if (frame < 128) {
        Palette_SetTone(Math16_Wrap(740 - frame * 5));
    } else {
        Grcg_Off();
        return true;
    }
    Grcg_Off();
    return false;
}

static const BOMB_STRATEGY *M_Strategy(const uint8_t type)
{
    static const BOMB_STRATEGY strategies[] = {
        { M_TypeA, 136 },
        { M_TypeB, 164 },
        { M_TypeC, 112 },
    };
    ASSERT(type < ARRAY_SIZE(strategies));
    return &strategies[type];
}

void Bomb_Init(void)
{
    memset(&m_Bomb, 0, sizeof(m_Bomb));
}

void Bomb_ResetStage(void)
{
    m_Bomb.stage_bombs = 0;
    m_Bomb.active = 0;
}

void Bomb_Activate(const PLAYFIELD_PAGES pages)
{
    if (m_Bomb.active != 0 || g_Score->bombs == 0) {
        return;
    }
    m_Bomb.active = 1;
    Player_BeginBomb();
    Score_UseBomb();
    Hud_Bombs(g_Score->bombs);
    m_Bomb.frame = 0;
    m_Bomb.stage_bombs++;
    Performance_CountBomb();
    SoundEffects_Play(SOUND_EFFECT_ATTACK_START);
    m_Bomb.center = (ENTITY_POINT) { 220, 196 };
    m_Bomb.circle_frame = 0;
    m_Bomb.circle_done = 0;
    M_Clear(pages);
}

void Bomb_Invalidate(
    const PLAYFIELD_PAGES pages, const BOMB_ASSETS *const assets)
{
    if (m_Bomb.active == 0) {
        return;
    }
    if (m_Bomb.circle_frame > 1) {
        m_Bomb.frame = Math16_Wrap((int32_t)m_Bomb.frame - 1);
        M_BombCircle(
            assets, pages.scroll, Math16_Wrap(256 - m_Bomb.frame * 8),
            (uint8_t)m_Bomb.frame, true);
        if (m_Bomb.frame >= 32) {
            m_Bomb.circle_frame = 0;
        }
        m_Bomb.frame = Math16_Wrap((int32_t)m_Bomb.frame + 1);
    }
    if (m_Bomb.circle_done == 1 && m_Bomb.frame <= 32) {
        Background_MarkAll();
    }
    const int16_t restore = M_Strategy(assets->shot_type)->restore_frame;
    if (m_Bomb.frame == restore || m_Bomb.frame == restore + 1) {
        Background_MarkAll();
    }
}

void Bomb_Update(
    const PLAYFIELD_PAGES pages, const BOMB_ASSETS *const assets,
    uint8_t *const slowdown, const bool reduce_effects,
    const uint8_t *const transition_tail, const bool fix_transition_overread)
{
    if (m_Bomb.active == 0) {
        return;
    }
    ASSERT(pages.back < 2);
    m_Bomb.frame = Math16_Wrap((int32_t)m_Bomb.frame + 1);
    bool done = false;
    if (m_Bomb.circle_done == 0) {
        if (m_Bomb.frame <= 32) {
            m_Bomb.circle_frame = Math16_Wrap((int32_t)m_Bomb.circle_frame + 1);
            m_Bomb.page_scroll[pages.back] = pages.scroll;
            Grcg_SetColor(15);
            M_BombCircle(
                assets, pages.scroll, Math16_Wrap(256 - m_Bomb.frame * 8),
                (uint8_t)m_Bomb.frame, false);
            Grcg_Off();
        } else if (m_Bomb.circle_frame == 0) {
            m_Bomb.frame = 0;
            m_Bomb.circle_done++;
        }
    } else if (m_Bomb.circle_done == 1) {
        done = M_Strategy(assets->shot_type)
                   ->update(
                       pages, assets, slowdown, reduce_effects, transition_tail,
                       fix_transition_overread);
    }
    if (done) {
        m_Bomb.active = 0;
        Player_EndBomb();
        Palette_SetTone(100);
    }
}
