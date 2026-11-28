#include "th01/boss/private.h"
#include "th01/stage.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

static void M_Whiteout(void)
{
    for (int32_t color = 0; color < 16; color++) {
        for (int32_t component = 0; component < 3; component++) {
            const int8_t value = g_Palette->saved.colors[color][component];
            if (value < 15) {
                Palette_SetGlobalSavedComponent(color, component, value + 1);
            }
        }
    }
    Palette_SetGlobal(&g_Palette->saved);
}

static bool M_Whitein(const PALETTE *const post_defeat)
{
    int32_t done = 0;
    for (int32_t color = 0; color < 16; color++) {
        for (int32_t component = 0; component < 3; component++) {
            const int8_t value = g_Palette->saved.colors[color][component];
            const int8_t target = post_defeat->colors[color][component];
            ASSERT(target >= 0 && target <= 15);
            if (value > target) {
                Palette_SetGlobalSavedComponent(color, component, value - 1);
            } else {
                done++;
            }
        }
    }
    Palette_SetGlobal(&g_Palette->saved);
    return done == 48;
}

static void M_Restore(BOSS_DEFEAT *const defeat, const PTN *const miko)
{
    defeat->frame = 0;
    Palette_SetGlobalGray(15);
    Video_SelectDrawingPage(1);
    Video_Copy(1, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 0, 0, 0);
    Video_SelectDrawingPage(0);
    Sprite_PtnPut(miko, 0, -1, g_Player->left, PLAYER_TOP);
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
    VSyncIrq_BeginWait(&defeat->wait);
}

bool BossDefeat_Resume(
    BOSS_DEFEAT *const defeat, const PALETTE *const post_defeat,
    const PTN *const miko, const int16_t *const origin_top)
{
    for (;;) {
        switch (defeat->phase) {
        case BOSS_DEFEAT_DONE:
            return true;
        case BOSS_DEFEAT_SCROLL_WAIT:
            if (!VSyncIrq_WaitReady(&defeat->wait)) {
                return false;
            }
            Video_SetScroll(0);
            defeat->top = Math16_Wrap(
                *origin_top + (defeat->kind == BOSS_DEFEAT_SINGYOKU ? 48 : 16));
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
                M_Restore(defeat, miko);
                break;
            }
            if (defeat->frame % (defeat->kind == BOSS_DEFEAT_SINGYOKU ? 15 : 7)
                == 0) {
                M_Whiteout();
            }
            defeat->phase = BOSS_DEFEAT_WHITEOUT_DELAY;
            break;
        }
        case BOSS_DEFEAT_WHITEOUT_DELAY:
            if (!VSyncIrq_DelayReady(1)) {
                return false;
            }
            defeat->phase = BOSS_DEFEAT_WHITEOUT;
            break;
        case BOSS_DEFEAT_WHITEIN:
            defeat->frame++;
            if (defeat->frame % (defeat->kind == BOSS_DEFEAT_SINGYOKU ? 15 : 5)
                    == 0
                && M_Whitein(post_defeat)) {
                if (defeat->kind == BOSS_DEFEAT_REGULAR) {
                    Stage_MarkCleared();
                    Player_Hit();
                }
                defeat->phase = BOSS_DEFEAT_DONE;
                return true;
            }
            defeat->phase = BOSS_DEFEAT_WHITEIN_DELAY;
            break;
        case BOSS_DEFEAT_WHITEIN_DELAY:
            if (!VSyncIrq_DelayReady(1)) {
                return false;
            }
            defeat->phase = BOSS_DEFEAT_WHITEIN;
            break;
        }
    }
}
