#include "th01/boss_defeat.h"

#include "th01/stage.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

static void M_Whiteout(const BOSS_DEFEAT_CONTEXT *const c)
{
    for (int32_t color = 0; color < 16; color++) {
        for (int32_t component = 0; component < 3; component++) {
            int8_t *const value = &g_Palette.saved.colors[color][component];
            if (*value < 15) {
                (*value)++;
            }
        }
    }
    Palette_Set(&g_Palette, &g_Palette.saved);
}

static bool M_Whitein(const BOSS_DEFEAT_CONTEXT *const c)
{
    int32_t done = 0;
    for (int32_t color = 0; color < 16; color++) {
        for (int32_t component = 0; component < 3; component++) {
            int8_t *const value = &g_Palette.saved.colors[color][component];
            const int8_t target = c->post_defeat->colors[color][component];
            ASSERT(target >= 0 && target <= 15);
            if (*value > target) {
                (*value)--;
            } else {
                done++;
            }
        }
    }
    Palette_Set(&g_Palette, &g_Palette.saved);
    return done == 48;
}

static void M_Restore(
    BOSS_DEFEAT *const defeat, const BOSS_DEFEAT_CONTEXT *const c)
{
    defeat->frame = 0;
    Palette_SetGray(&g_Palette, 15);
    Video_SelectDrawingPage(1);
    Video_Copy(1, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 0, 0, 0);
    Video_SelectDrawingPage(0);
    Sprite_PtnPut(c->miko, 0, -1, g_Player.left, PLAYER_TOP);
    defeat->phase = BOSS_DEFEAT_WHITEIN;
}

RESULT BossDefeat_ValidateSprites(const PTN *const miko)
{
    FAIL_IF(
        miko->count < 1 || miko->pixels == nullptr,
        "Missing default player sprite");
    return OK;
}

void BossDefeat_Begin(BOSS_DEFEAT *const defeat, const BOSS_DEFEAT_KIND kind)
{
    ASSERT(defeat->phase == BOSS_DEFEAT_DONE);
    ASSERT(kind == BOSS_DEFEAT_REGULAR || kind == BOSS_DEFEAT_SINGYOKU);
    *defeat = (BOSS_DEFEAT) { .phase = BOSS_DEFEAT_SCROLL_WAIT, .kind = kind };
    Frame_BeginWait(&defeat->wait);
}

bool BossDefeat_Resume(
    BOSS_DEFEAT *const defeat, const BOSS_DEFEAT_CONTEXT *const c)
{
    for (;;) {
        switch (defeat->phase) {
        case BOSS_DEFEAT_DONE:
            return true;
        case BOSS_DEFEAT_SCROLL_WAIT:
            if (!Frame_WaitReady(&defeat->wait)) {
                return false;
            }
            Video_SetScroll(0);
            defeat->top = Math16_Wrap(
                *c->origin_top
                + (defeat->kind == BOSS_DEFEAT_SINGYOKU ? 48 : 16));
            ASSERT(defeat->top >= 0 && defeat->top < VIDEO_HEIGHT);
            defeat->bottom = defeat->top;
            defeat->phase = BOSS_DEFEAT_WHITEOUT;
            break;
        case BOSS_DEFEAT_WHITEOUT: {
            if (defeat->top >= 0) {
                Video_HorizontalLine(0, VIDEO_WIDTH - 1, defeat->top, 7);
            }
            if (defeat->bottom < VIDEO_HEIGHT) {
                Video_HorizontalLine(0, VIDEO_WIDTH - 1, defeat->bottom, 7);
            }
            const int16_t distance =
                defeat->kind == BOSS_DEFEAT_SINGYOKU ? 1 : 2;
            defeat->top -= distance;
            defeat->bottom += distance;
            defeat->frame++;
            if (defeat->top < 0 && defeat->bottom >= VIDEO_HEIGHT) {
                M_Restore(defeat, c);
                break;
            }
            if (defeat->frame % (defeat->kind == BOSS_DEFEAT_SINGYOKU ? 15 : 7)
                == 0) {
                M_Whiteout(c);
            }
            defeat->phase = BOSS_DEFEAT_WHITEOUT_DELAY;
            break;
        }
        case BOSS_DEFEAT_WHITEOUT_DELAY:
            if (!Frame_DelayReady(1)) {
                return false;
            }
            defeat->phase = BOSS_DEFEAT_WHITEOUT;
            break;
        case BOSS_DEFEAT_WHITEIN:
            defeat->frame++;
            if (defeat->frame % (defeat->kind == BOSS_DEFEAT_SINGYOKU ? 15 : 5)
                    == 0
                && M_Whitein(c)) {
                if (defeat->kind == BOSS_DEFEAT_REGULAR) {
                    g_Stage.cleared = true;
                    g_Player.is_hit = true;
                }
                defeat->phase = BOSS_DEFEAT_DONE;
                return true;
            }
            defeat->phase = BOSS_DEFEAT_WHITEIN_DELAY;
            break;
        case BOSS_DEFEAT_WHITEIN_DELAY:
            if (!Frame_DelayReady(1)) {
                return false;
            }
            defeat->phase = BOSS_DEFEAT_WHITEIN;
            break;
        }
    }
}
