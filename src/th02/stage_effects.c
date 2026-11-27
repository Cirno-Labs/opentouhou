#include "th02/background.h"
#include "th02/clip.h"
#include "th02/draw.h"
#include "th02/grcg.h"
#include "th02/palette.h"
#include "th02/random.h"
#include "th02/sound_effects.h"
#include "th02/sparks.h"
#include "th02/stage_effects_private.h"
#include "th02/video.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

static STAGE_EFFECTS m_StageEffects;
STAGE_EFFECTS *const p_StageEffects = &m_StageEffects;
const STAGE_EFFECTS *const g_StageEffects = &m_StageEffects;

static void M_Lightning(void)
{
    if (m_StageEffects.lightning_frame == 0) {
        return;
    }
    if (m_StageEffects.lightning_frame == 1) {
        m_StageEffects.lightning_next = 100;
    }
    m_StageEffects.lightning_frame++;
    const uint16_t frame = m_StageEffects.lightning_frame;
    const uint16_t next = m_StageEffects.lightning_next;
    if (frame >= next && (uint16_t)(next + 8) > frame) {
        Palette_SetTone((frame & 1) != 0 ? 140 : 100);
        if ((uint16_t)(next + 7) == frame) {
            Palette_SetTone(100);
            m_StageEffects.lightning_next += (Random_RingU16() & 0x3ff) + 20;
        }
    }
}

static void M_Add(const int16_t left)
{
    for (size_t i = 0; i < STAGE_PARTICLE_COUNT; i++) {
        STAGE_PARTICLE *const p = &m_StageEffects.particles[i];
        if (p->state == ENTITY_FREE) {
            p->state = ENTITY_ACTIVE;
            p->position[0] = p->position[1] = (ENTITY_POINT) { left, 16 };
            p->speed = Random_RingU8And(3) + 3;
            return;
        }
    }
}

static void M_Dot(const int16_t left, const uint16_t top, const uint8_t color)
{
    const uint8_t shift = (uint16_t)left & 7;
    const uint16_t mask =
        (uint16_t)((0xc0u >> shift) | (0xc0u << (16 - shift)));
    const uint16_t scaled = (uint16_t)(top << 6);
    uint16_t offset = (uint16_t)(left / 8 + scaled + (scaled >> 2));
    for (unsigned row = 0; row < 2; row++) {
        for (unsigned lane = 0; lane < 2; lane++) {
            uint8_t *const pixels = g_Video.pages[g_Video.drawing_page]
                + (size_t)((uint16_t)(offset + lane) & 0x7fff) * 8;
            const uint8_t bits = (uint8_t)(mask >> (lane * 8));
            for (unsigned bit = 0; bit < 8; bit++) {
                if ((bits & (0x80u >> bit)) != 0) {
                    pixels[bit] = color;
                }
            }
        }
        offset = (uint16_t)(offset + 80);
    }
}

static void M_Particles(const PLAYFIELD_PAGES pages, const uint8_t color)
{
    Grcg_SetColor(color);
    for (size_t i = 0; i < STAGE_PARTICLE_COUNT; i++) {
        STAGE_PARTICLE *const p = &m_StageEffects.particles[i];
        if (p->state != ENTITY_ACTIVE) {
            continue;
        }
        ENTITY_POINT *const point = &p->position[pages.back];
        point->x = Math16_Wrap((int32_t)point->x - p->speed);
        point->y = Math16_Wrap((int32_t)point->y + p->speed);
        if (point->x < 32) {
            p->state = ENTITY_REMOVE;
        } else if (point->y < 384 && point->x < 416) {
            uint16_t top = (uint16_t)(point->y + pages.scroll);
            if (top >= 400) {
                top -= 400;
            }
            M_Dot(point->x, top, color);
        }
    }
    Grcg_Off();
}

static void M_Storm(const PLAYFIELD_PAGES pages, const bool reduce_effects)
{
    if (m_StageEffects.storm_frame < 999 && m_StageEffects.storm_frame != 0) {
        m_StageEffects.storm_frame =
            Math16_Wrap((int32_t)m_StageEffects.storm_frame + 1);
        const int16_t frame = m_StageEffects.storm_frame;
        Palette_SetColor(
            8, 176, (uint8_t)(frame / 16 + 144), (uint8_t)(frame / 8 + 144));
        const uint8_t thirteen = (uint8_t)(160 - frame / 16);
        Palette_SetColor(13, thirteen, thirteen, (uint8_t)(255 - frame / 4));
        if (thirteen <= 128) {
            m_StageEffects.storm_frame = 1000;
            Clip_SetPlayfield();
            Palette_SetColor(14, 224, 192, 176);
            Sparks_SetDrift(-1);
            m_StageEffects.lightning_frame = 1;
        }
        Palette_Show();
        return;
    }
    if (m_StageEffects.storm_frame >= 1000) {
        M_Lightning();
        m_StageEffects.storm_frame =
            Math16_Wrap((int32_t)m_StageEffects.storm_frame + 1);
        if (1002 + reduce_effects * 2 < m_StageEffects.storm_frame) {
            const int16_t left = (Random_RingU16() % 640) + 48;
            M_Add(left);
            m_StageEffects.storm_frame = 1000;
        }
        M_Particles(pages, 8);
    }
}

static void M_Rain(const PLAYFIELD_PAGES pages)
{
    if (m_StageEffects.rain_clip_reset) {
        Clip_SetPlayfield();
        m_StageEffects.rain_clip_reset = false;
        Sparks_SetDrift(-1);
    }
    m_StageEffects.rain_frame++;
    if ((m_StageEffects.rain_frame & 1) == 0) {
        const int16_t left = (Random_RingU16() % 640) + 48;
        M_Add(left);
    }
    M_Lightning();
    M_Particles(pages, 3);
}

static void M_Vortex(const PLAYFIELD_PAGES pages, uint8_t *const slowdown)
{
    m_StageEffects.vortex_frame++;
    const uint16_t frame = m_StageEffects.vortex_frame;
    if (frame < 168) {
        return;
    }
    if (frame == 168) {
        SoundEffects_Play(SOUND_EFFECT_BLAST);
    } else if (frame < 200) {
        if (frame <= 180) {
            Palette_SetTone((frame & 1) * 70 + 100);
        }
        const uint16_t offset = frame & 1;
        Video_SetSplit(400 - pages.scroll, pages.scroll * 40 + offset, offset);
        *slowdown = 3;
        m_StageEffects.center_x = 224;
        m_StageEffects.center_y = 200;
        m_StageEffects.radius = 0;
        m_StageEffects.angle = 0;
    } else if (frame < 1000) {
        if (frame == 200) {
            *slowdown = 1;
        }
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        if (frame < 400) {
            r = (uint8_t)(200 - (frame >> 1));
            m_StageEffects.radius = frame - 200;
            m_StageEffects.angle++;
        } else if (frame < 600) {
            g = (uint8_t)(44 - (frame >> 1));
            m_StageEffects.radius = frame - 400;
            m_StageEffects.angle--;
        } else if (frame < 800) {
            b = (uint8_t)(144 - (frame >> 1));
            m_StageEffects.radius = frame - 600;
            m_StageEffects.angle++;
        } else {
            r = b = (uint8_t)(244 - (frame >> 1));
            m_StageEffects.radius = frame - 800;
            m_StageEffects.angle--;
        }
        Palette_SetColor(0, r, g, b);
    } else {
        m_StageEffects.vortex_frame = 200;
    }
    Palette_Show();
    Grcg_SetColor(7);
    for (unsigned i = 0; i < 64; i++) {
        const uint8_t angle = m_StageEffects.angle + i * 4;
        const int16_t left = Math16_PolarX(
            m_StageEffects.center_x, m_StageEffects.radius, angle);
        const int16_t top = Math16_PolarY(
            m_StageEffects.center_y, m_StageEffects.radius, angle);
        m_StageEffects.dots[pages.back][i] = (ENTITY_POINT) { left, top };
        uint16_t rolled = (uint16_t)(top + pages.scroll);
        if (rolled >= 400) {
            rolled -= 400;
        }
        Draw_BoxFill(g_Clip, left, (int16_t)rolled, left, (int16_t)rolled);
    }
    Grcg_Off();
}

static void M_Portal(const PLAYFIELD_PAGES pages)
{
    if (pages.back != 0) {
        return;
    }
    ASSERT(g_Background->cache.mode != TILE_BLACK);
    ASSERT(g_Video.drawing_page == 0);
    Background_Mark(pages.scroll, 176, 156, 96, 96);
    Background_RestoreIntensity();
    m_StageEffects.portal_top = (uint16_t)(152 + pages.scroll);
    if (m_StageEffects.portal_top >= 400) {
        m_StageEffects.portal_top -= 400;
    }
    uint16_t top = m_StageEffects.portal_top;
    uint8_t *const page = g_Video.pages[0];
    for (unsigned tile_row = 0; tile_row < 6; tile_row++) {
        for (unsigned column = 0; column < 6; column++) {
            uint16_t source = Background_Source(tile_row * 16 + column);
            ASSERT((uint32_t)source + 15 * 80 + 1 < VIDEO_PLANE_SIZE);
            uint16_t destination =
                (uint16_t)((176 + column * 16) / 8 + top * 80);
            for (unsigned row = 0; row < 16; row++) {
                uint8_t pixels[16];
                memcpy(pixels, page + source * 8, sizeof(pixels));
                const unsigned swap = ((source ^ destination) & 1) * 8;
                for (unsigned pixel = 0; pixel < 16; pixel++) {
                    page[destination * 8 + pixel] |= pixels[pixel ^ swap] & 8;
                }
                source += 80;
                destination += 80;
                if (destination > 32000) {
                    destination -= 32000;
                }
            }
        }
        top += 16;
        if (top >= 400) {
            top -= 400;
        }
    }
    Grcg_Off();
}

void StageEffects_Init(void)
{
    memset(&m_StageEffects, 0, sizeof(m_StageEffects));
    m_StageEffects.rain_clip_reset = true;
}

void StageEffects_ResetStage(const uint8_t stage)
{
    m_StageEffects.stage = stage;
    m_StageEffects.vortex_frame = 0;
}

void StageEffects_Invalidate(const PLAYFIELD_PAGES pages)
{
    ASSERT(pages.back < 2 && pages.front < 2);
    if (m_StageEffects.stage < 2) {
        for (size_t i = 0; i < STAGE_PARTICLE_COUNT; i++) {
            STAGE_PARTICLE *const p = &m_StageEffects.particles[i];
            if (p->state == ENTITY_FREE) {
                continue;
            }
            const ENTITY_POINT point = p->position[pages.back];
            Background_Mark(pages.scroll, point.x, point.y, 2, 3);
            if (p->state == ENTITY_REMOVE) {
                p->state = ENTITY_FREE;
            } else {
                p->position[pages.back] = p->position[pages.front];
            }
        }
    } else if (m_StageEffects.stage == 2) {
        for (unsigned i = 0; i < 64; i++) {
            const ENTITY_POINT point = m_StageEffects.dots[pages.back][i];
            Background_Mark(pages.scroll, point.x, point.y, 1, 2);
        }
    }
}

void StageEffects_Update(
    const PLAYFIELD_PAGES pages, const uint8_t stage, const bool reduce_effects,
    uint8_t *const slowdown)
{
    m_StageEffects.stage = stage;
    if (stage == 0) {
        M_Storm(pages, reduce_effects);
    } else if (stage == 1) {
        M_Rain(pages);
    } else if (stage == 2) {
        M_Vortex(pages, slowdown);
    } else if (stage == 3) {
        M_Portal(pages);
    }
}

void StageEffects_StartStorm(void)
{
    m_StageEffects.storm_frame = 1;
}

void StageEffects_StartLightning(void)
{
    m_StageEffects.lightning_frame = 1;
}

void StageEffects_StopLightning(void)
{
    m_StageEffects.lightning_frame = 0;
}
