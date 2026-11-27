#include "th02/boss/explosion.h"
#include "th02/background.h"
#include "th02/stage.h"
#include "th02/sound_effects.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

static void M_Sides(void)
{
    CLIP clip = *g_Clip;
    clip.left = 0;
    clip.right = VIDEO_WIDTH - 1;
    Grcg_SetColor(0);
    const uint8_t pages[] = { g_Stage->scroll.front, g_Stage->scroll.back };
    for (size_t i = 0; i < ARRAY_SIZE(pages); i++) {
        Video_AccessPage(pages[i]);
        Draw_BoxFill(&clip, 16, 0, 31, 399);
        Draw_BoxFill(&clip, 416, 0, 431, 399);
    }
    Grcg_Off();
}

static void M_Circle(
    const int16_t center_x, const int16_t center_y, const int16_t radius,
    const bool cleanup)
{
    const uint8_t size = (uint8_t)((radius >> 6) + 1);
    for (unsigned angle = 0; angle < 256; angle += 2) {
        const int16_t left = Math16_PolarX(center_x, radius, angle);
        const int16_t top = Math16_PolarY(center_y, radius, angle);
        if (left <= 24 || left >= 416 || top <= 8 || top >= 384) {
            continue;
        }
        if (cleanup) {
            Background_Mark(g_Stage->scroll.line, left, top, size, size);
        } else {
            int16_t rolled = Math16_Wrap((int32_t)top + g_Stage->scroll.line);
            if (rolled >= 400) {
                rolled -= 400;
            }
            Draw_Dot(left, rolled, size);
        }
    }
}

static void M_Burst(
    const PATTERN_BANK *const patterns, const uint8_t skew,
    const int16_t center_x, const int16_t center_y, const int16_t frame,
    const bool cleanup)
{
    const int16_t radius = frame * 16;
    const uint16_t image = frame / 6 + 88;
    for (unsigned i = 0; i < 256; i += 4) {
        const uint8_t angle = (uint8_t)(i + frame);
        const int16_t left = Math16_PolarX(center_x, radius, angle);
        const int16_t top = Math16_PolarY(center_y, radius, angle + skew);
        if (left < 16 || left >= 416 || top <= 0 || top >= 384) {
            continue;
        }
        if (cleanup) {
            Background_Mark(g_Stage->scroll.line, left, top, 16, 16);
        } else if (left > 16) {
            int16_t rolled = Math16_Wrap((int32_t)top + g_Stage->scroll.line);
            if (rolled >= 400) {
                rolled -= 400;
            }
            const PATTERN *const pattern = &patterns->entries[image];
            ASSERT(pattern->data != nullptr);
            Draw_SpriteRollTiny(pattern, left, rolled);
        }
    }
}

void BossExplosion_Draw(
    const PATTERN_BANK *const patterns, const uint8_t angle, int16_t center_x,
    const int16_t center_y, int16_t frame)
{
    if (frame < 0 || frame >= 58) {
        return;
    }
    ASSERT(g_Stage->scroll.front < 2 && g_Stage->scroll.back < 2);
    ASSERT(g_Stage->scroll.line >= 0 && g_Stage->scroll.line <= 400);
    if (frame == 0) {
        M_Sides();
    }
    if (frame < 32) {
        if (frame == 1) {
            SoundEffects_Play(SOUND_EFFECT_EXPLOSION);
        }
        center_x = Math16_Wrap((int32_t)center_x + 7);
        if (frame >= 2) {
            M_Circle(
                center_x, center_y, (int16_t)(240 - (frame - 2) * 8), true);
        }
        if (frame < 30) {
            Grcg_SetColor((uint8_t)((frame & 1) * 11 + 4));
            M_Circle(center_x, center_y, (int16_t)(240 - frame * 8), false);
            Grcg_Off();
        }
        return;
    }
    frame -= 32;
    if (frame == 0) {
        SoundEffects_Play(SOUND_EFFECT_BLAST);
    }
    if (frame >= 2) {
        M_Burst(patterns, angle, center_x, center_y, frame - 2, true);
        Background_RepaintDirty();
    }
    Stage_SetSlowdown(1);
    if (frame >= 24) {
        return;
    }
    M_Burst(patterns, angle, center_x, center_y, frame, false);
    Stage_SetSlowdown(2);
    if (frame <= 8) {
        Palette_SetTone((int16_t)((frame & 1) * 70 + 100));
    }
    if (frame <= 20) {
        const uint16_t offset = frame & 1;
        Video_SetSplit(
            400 - g_Stage->scroll.line,
            (uint16_t)(g_Stage->scroll.line * 40 + offset), offset);
        Stage_SetSlowdown(3);
    }
}
