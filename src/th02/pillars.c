#include "th02/pillars.h"
#include "th02/background.h"
#include "th02/draw.h"
#include "th02/player.h"
#include "th02/sound_effects.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

static PILLARS m_Pillars;
const PILLARS *const g_Pillars = &m_Pillars;

static void M_Put(
    const PATTERN_BANK *const patterns, const int16_t left, const int16_t top,
    const uint16_t pattern, const bool tiny)
{
    ASSERT(pattern < PATTERN_LIMIT);
    const PATTERN *const image = &patterns->entries[pattern];
    ASSERT(image->data != nullptr);
    if (tiny) {
        Draw_SpriteRollTiny(image, left, top);
    } else {
        Draw_SpriteRoll(image, left, top);
    }
}

static void M_Shaft(
    const PILLAR *const pillar, const int16_t scroll,
    const PATTERN_BANK *const patterns)
{
    int16_t top = m_Pillars.current.y;
    int16_t rolled = Math16_Wrap((int32_t)top + scroll);
    if (rolled >= 400) {
        rolled -= 400;
    }
    if (pillar->phase < 5) {
        M_Put(patterns, m_Pillars.current.x, rolled, pillar->phase + 91, true);
    }
    rolled = Math16_Wrap((int32_t)rolled + 8);
    if (rolled >= 400) {
        rolled -= 400;
    }
    top = Math16_Wrap((int32_t)top + 8);
    const uint16_t pattern = pillar->pattern + pillar->phase;
    while (top < 384) {
        M_Put(patterns, m_Pillars.current.x, rolled, pattern, true);
        rolled = Math16_Wrap((int32_t)rolled + 16);
        if (rolled >= 400) {
            rolled -= 400;
        }
        top = Math16_Wrap((int32_t)top + 16);
    }
    if (pillar->phase == 4
        && Math16_Wrap((int32_t)m_Pillars.current.x - 24) < g_Player->current.x
        && Math16_Wrap((int32_t)m_Pillars.current.x + 8) > g_Player->current.x
        && g_Player->current.y >= m_Pillars.current.y && g_Player->hit == 0) {
        Player_Hit();
    }
}

void Pillars_Init(void)
{
    memset(&m_Pillars, 0, sizeof(m_Pillars));
}

void Pillars_ResetStage(const uint8_t stage)
{
    ASSERT(stage < 6);
    for (size_t i = 0; i < PILLAR_COUNT; i++) {
        m_Pillars.slots[i].state = ENTITY_FREE;
    }
    m_Pillars.frames_new = 16;
    m_Pillars.enabled = stage == 3 || stage == 5;
}

void Pillars_Enable(void)
{
    m_Pillars.enabled = true;
}

void Pillars_SetSpawnInterval(const uint8_t frames)
{
    m_Pillars.frames_new = frames;
}

void Pillars_Add(
    const int16_t left, const int16_t top, const int16_t duration,
    const int16_t pattern)
{
    if (left < 32 || left >= 416) {
        return;
    }
    for (size_t i = 0; i < PILLAR_COUNT; i++) {
        PILLAR *const p = &m_Pillars.slots[i];
        if (p->state == ENTITY_FREE) {
            p->state = ENTITY_ACTIVE;
            p->phase = 1;
            p->position = (ENTITY_POINT) { left, top };
            p->frames = m_Pillars.frames_new;
            p->duration = duration;
            p->cel = 0;
            p->pattern = (uint8_t)pattern;
            SoundEffects_Play(SOUND_EFFECT_PILLAR);
            return;
        }
    }
}

void Pillars_Invalidate(const PLAYFIELD_PAGES pages, const uint32_t frame)
{
    if (!m_Pillars.enabled) {
        return;
    }
    for (size_t i = 0; i < PILLAR_COUNT; i++) {
        PILLAR *const p = &m_Pillars.slots[i];
        if (p->state == ENTITY_FREE) {
            continue;
        }
        const bool startup = p->cel <= 4;
        Background_Mark(
            pages.scroll,
            startup ? Math16_Wrap((int32_t)p->position.x - 8) : p->position.x,
            startup ? Math16_Wrap((int32_t)p->position.y - 8) : p->position.y,
            startup ? 32 : 16, startup ? 32 : Math16_Wrap(400 - p->position.y));
        if (startup && (frame & 3) == 0) {
            p->cel++;
        }
        if (p->state == ENTITY_REMOVE) {
            p->state = ENTITY_FREE;
        }
    }
}

void Pillars_Update(
    const PLAYFIELD_PAGES pages, const uint32_t frame,
    const PATTERN_BANK *const patterns)
{
    if (!m_Pillars.enabled) {
        return;
    }
    for (size_t i = 0; i < PILLAR_COUNT; i++) {
        PILLAR *const p = &m_Pillars.slots[i];
        if (p->state != ENTITY_ACTIVE) {
            continue;
        }
        m_Pillars.current = p->position;
        if (p->cel < 4) {
            int16_t top =
                Math16_Wrap((int32_t)p->position.y - 8 + pages.scroll);
            if (top >= 400) {
                top -= 400;
            }
            M_Put(
                patterns, Math16_Wrap((int32_t)p->position.x - 8), top,
                p->cel + 30, false);
            p->frames = Math16_Wrap((int32_t)p->frames - 1);
            continue;
        }
        if (p->phase == 1) {
            p->frames = Math16_Wrap((int32_t)p->frames - 1);
            if (p->frames <= 0) {
                p->phase++;
            }
        } else if (p->phase < 4) {
            if ((frame & 7) == 0) {
                p->phase++;
            }
            if (p->phase == 4) {
                SoundEffects_Play(SOUND_EFFECT_PILLAR_FALL);
            }
        } else if (p->phase == 4) {
            p->duration = Math16_Wrap((int32_t)p->duration - 1);
            if (p->duration == 0) {
                p->phase++;
            }
        } else {
            if ((frame & 3) == 0) {
                p->phase++;
            }
            if (p->phase == 9) {
                p->state = ENTITY_REMOVE;
                continue;
            }
        }
        M_Shaft(p, pages.scroll, patterns);
    }
}
