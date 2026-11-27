#include "th02/dialogue/slide.h"

#include "th02/background.h"
#include "th02/grcg.h"
#include "th02/hud.h"
#include "th02/palette.h"
#include "th02/player.h"
#include "th02/score.h"
#include "th02/stage.h"
#include "th02/video.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

static int16_t M_Top(const int16_t top, const int16_t scroll)
{
    const int16_t sum = Math16_Wrap((int32_t)top + scroll);
    return sum >= 400 ? sum - 400 : sum;
}

static void M_Begin(
    DIALOGUE_SLIDE *const slide, const PATTERN_BANK *const patterns,
    const bool leave)
{
    ASSERT(g_Stage->scroll.front < 2 && g_Stage->scroll.back < 2);
    ASSERT(g_Stage->scroll.line >= 0 && g_Stage->scroll.line < 400);
    ASSERT(g_Grcg->mode == 0);
    slide->patterns = patterns;
    slide->leave = leave;
    slide->pending = false;
    slide->done = false;
    slide->left = leave ? 32 : 400;
    slide->top = M_Top(320, g_Stage->scroll.line);
    slide->bottom = M_Top(32, slide->top);
}

static void M_Put(
    const PATTERN_BANK *const patterns, const uint16_t image,
    const int16_t left, const int16_t top, const bool tiny)
{
    const PATTERN *const pattern = &patterns->entries[image];
    ASSERT(pattern->data != nullptr);
    if (tiny) {
        Draw_SpriteRollTiny(pattern, left, top);
    } else {
        Draw_SpriteRoll(pattern, left, top);
    }
}

static int16_t M_Draw(const DIALOGUE_SLIDE *const slide)
{
    const PATTERN_BANK *const patterns = slide->patterns;
    Background_Mark(g_Stage->scroll.line, 32, 320, 384, 80);
    Background_RepaintDirty();
    M_Put(
        patterns, 0, g_Player->current.x,
        M_Top(g_Player->current.y, g_Stage->scroll.line), false);
    const ENTITY_POINT option = g_Player->option[g_Stage->scroll.back];
    const int16_t top = M_Top(option.y, g_Stage->scroll.line);
    M_Put(patterns, 72, option.x, top, true);
    M_Put(patterns, 72, Math16_Wrap((int32_t)option.x + 48), top, true);
    int16_t left = slide->left;
    for (uint16_t part = 0; part < 12; part++) {
        const uint16_t image = part < 3 ? 34 + part : part == 11 ? 38 : 37;
        if (left >= 16 && left <= 400) {
            M_Put(patterns, image, left, slide->top, false);
            M_Put(patterns, image + 8, left, slide->bottom, false);
        }
        left += 32;
    }
    return left;
}

void DialogueSlide_BeginEnter(
    DIALOGUE_SLIDE *const slide, const PATTERN_BANK *const patterns)
{
    memset(slide, 0, sizeof(*slide));
    Hud_WipePlayfield();
    Score_GrantBonus();
    Video_ScrollUp(g_Stage->scroll.line);
    Palette_SetTone(100);
    Video_AccessPage(g_Stage->scroll.front);
    Hud_WipePlayfield();
    M_Begin(slide, patterns, false);
    slide->restore_disabled = g_Background->cache.mode == TILE_DISABLED;
    if (slide->restore_disabled) {
        Background_SetMode(TILE_BLACK);
    }
}

void DialogueSlide_BeginLeave(
    DIALOGUE_SLIDE *const slide, const PATTERN_BANK *const patterns)
{
    Hud_WipePlayfield();
    M_Begin(slide, patterns, true);
}

DIALOGUE_SLIDE_RESULT DialogueSlide_Update(DIALOGUE_SLIDE *const slide)
{
    if (slide->done) {
        return DIALOGUE_SLIDE_DONE;
    }
    if (slide->pending) {
        const int16_t end = M_Draw(slide);
        if (slide->leave ? end < 48 : slide->left <= 32) {
            if (slide->leave) {
                if (slide->restore_disabled) {
                    Background_SetMode(TILE_DISABLED);
                    slide->restore_disabled = false;
                }
                Video_AccessPage(g_Stage->scroll.back);
            }
            slide->done = true;
            return DIALOGUE_SLIDE_DONE;
        }
    }
    slide->left -= 16;
    slide->pending = true;
    return DIALOGUE_SLIDE_FRAME;
}
