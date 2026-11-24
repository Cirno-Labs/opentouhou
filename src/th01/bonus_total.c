#include "th01/bonus.h"
#include "th01/session.h"
#include "th01/sound.h"

#include "pc98/zun/math16.h"
#include "th01/sprite.h"

#include "core/base/assert.h"

#include <string.h>

BONUS_TOTAL g_BonusTotal;

static void M_PutDigit(
    const BONUS_TOTAL_ASSETS *const assets, const int16_t digit)
{
    const int16_t left =
        (int16_t)(224
                  + (g_BonusTotal.place - (g_BonusTotal.digit_count == 6))
                      * 16);
    Sprite_Restore16(left, g_BonusTotal.top, 16, 16);
    if (digit >= 0 && digit <= 19) {
        Sprite_PtnPut(
            &assets->numerals, 10 + (digit % 10) / 4, digit % 4, left,
            g_BonusTotal.top);
    }
}

static int32_t M_Add(int32_t value, const int32_t cap)
{
    if (value > cap) {
        value = cap;
    }
    for (int32_t i = 0; i < 10; i++) {
        g_Bonus.score_bonus += (uint32_t)value;
    }
    return value;
}

static void M_Prepare(const uint64_t host_tick)
{
    int32_t value;
    if (g_BonusTotal.metric == 4) {
        g_Stage.score += g_Bonus.score_bonus;
        const int16_t div = Math16_Wrap((int32_t)(g_Bonus.score_bonus / 10000));
        const int16_t rem = (int16_t)(g_Bonus.score_bonus % 10000);
        g_BonusTotal.digits[0] = (int16_t)(div / 10);
        g_BonusTotal.digits[1] = (int16_t)(div % 10);
        g_BonusTotal.digits[2] = (int16_t)(rem / 1000);
        g_BonusTotal.digits[3] = (int16_t)(rem % 1000 / 100);
        g_BonusTotal.digits[4] = (int16_t)(rem % 100 / 10);
        g_BonusTotal.digits[5] = (int16_t)(rem % 10);
        g_BonusTotal.top = 196;
        g_BonusTotal.digit_count = 6;
    } else {
        switch (g_BonusTotal.metric) {
        case 0:
            value = (uint16_t)(g_StageTimer.time * 5);
            break;
        case 1:
            value = Math16_Wrap(g_Stage.maximum_combo * 200);
            break;
        case 2:
            value = Math16_Wrap(g_Session.lives * 500);
            value += Math16_Wrap(g_Player.remaining_bombs * 200);
            break;
        default:
            value = Math16_Wrap(g_BonusTotal.stage_number * 1000);
            break;
        }
        value = M_Add(value, g_BonusTotal.metric == 3 ? 65530 : 6553);
        g_BonusTotal.top = (int16_t)(86 + g_BonusTotal.metric * 21);
        g_BonusTotal.digit_count = g_BonusTotal.metric == 3 ? 6 : 5;
        int32_t divisor = g_BonusTotal.metric == 3 ? 10000 : 1000;
        for (int32_t i = 0; i < g_BonusTotal.digit_count; i++) {
            if (divisor == 0) {
                g_BonusTotal.digits[i] = 0;
            } else {
                g_BonusTotal.digits[i] = (int16_t)(value / divisor);
                value %= divisor;
                divisor /= 10;
            }
        }
        if (g_BonusTotal.metric == 3) {
            g_BonusTotal.digits[4] = 0;
        }
    }
    g_BonusTotal.place = 0;
    g_BonusTotal.spin = 0;
    if (g_BonusTotal.metric == 0) {
        g_BonusTotal.credit = 49;
        g_BonusTotal.last_tick = host_tick;
    }
    g_BonusTotal.phase = BONUS_TOTAL_DIGIT;
}

RESULT Bonus_ValidateTotalAssets(const BONUS_TOTAL_ASSETS *const a)
{
    FAIL_IF(
        a == nullptr || a->background.pixels == nullptr
            || a->background.width != VIDEO_WIDTH
            || a->background.height != VIDEO_HEIGHT
            || !a->background.has_palette || a->numerals.pixels == nullptr
            || a->numerals.count < 13,
        "Missing total bonus assets");
    return OK;
}

RESULT Bonus_LoadTotalAssets(
    VFS *const vfs, const ALLOCATOR *const allocator,
    BONUS_TOTAL_ASSETS *const out)
{
    *out = (BONUS_TOTAL_ASSETS) {};
    BONUS_TOTAL_ASSETS loaded = {};
    READER reader;
    MUST(Vfs_Open(vfs, "CLEAR3.GRP", VFS_IN_MEMORY, &reader));
    RESULT result = Pi_Load(&reader, allocator, &loaded.background);
    Reader_Close(&reader);
    MUST(result);
    MUST(Vfs_Open(vfs, "NUMB.PTN", VFS_IN_MEMORY, &reader));
    result = Ptn_Load(&reader, allocator, &loaded.numerals);
    Reader_Close(&reader);
    MUST(result);
    Palette_Import(&loaded.palette, loaded.background.palette);
    MUST(Bonus_ValidateTotalAssets(&loaded));
    *out = loaded;
    return OK;
}

bool Bonus_BeginTotal(
    const BONUS_TOTAL_ASSETS *const assets, const SHOTS_OUTPUT shots_output,
    const int16_t stage_number, const TH01_INPUT *const raw,
    const uint64_t host_tick)
{
    ASSERT(
        g_BonusTotal.phase == BONUS_TOTAL_DONE && stage_number >= 5
        && stage_number <= 20 && stage_number % 5 == 0);
    memset(&g_BonusTotal, 0, sizeof(g_BonusTotal));
    g_BonusTotal.phase = BONUS_TOTAL_INITIAL_WAIT;
    g_BonusTotal.stage_number = stage_number;
    g_BonusTotal.last_tick = host_tick;
    Shots_EraseAndReset(shots_output);
    Pellets_EraseAndResetMoving();
    return Bonus_ResumeTotal(assets, shots_output, raw, host_tick);
}

bool Bonus_ResumeTotal(
    const BONUS_TOTAL_ASSETS *const assets, const SHOTS_OUTPUT shots_output,
    const TH01_INPUT *const raw, const uint64_t host_tick)
{
    for (;;) {
        switch (g_BonusTotal.phase) {
        case BONUS_TOTAL_DONE:
            return true;
        case BONUS_TOTAL_INITIAL_WAIT: {
            if (!Frame_DelayReady(50)) {
                return false;
            }
            const VIDEO_IMAGE image = { .pixels = assets->background.pixels,
                                        .width = VIDEO_WIDTH,
                                        .height = VIDEO_HEIGHT,
                                        .stride = VIDEO_WIDTH,
                                        .key = -1 };
            Video_SelectDrawingPage(1);
            Video_Blit(
                &image, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 0, 0);
            g_Palette.resource = assets->palette;
            Video_SelectDrawingPage(0);
            Bonus_BeginRowTransition(&g_BonusTotal.transition);
            g_BonusTotal.phase = BONUS_TOTAL_TRANSITION;
            break;
        }
        case BONUS_TOTAL_TRANSITION:
            if (!Bonus_ResumeRowTransition(
                    &g_BonusTotal.transition, &g_Palette,
                    &g_Palette.resource)) {
                return false;
            }
            Sprite_PtnPut(
                &assets->numerals, g_BonusTotal.stage_number / 10, -1, 224, 14);
            Sprite_PtnPut(
                &assets->numerals, g_BonusTotal.stage_number % 10, -1, 256, 14);
            g_BonusTotal.phase = BONUS_TOTAL_STAGE_WAIT;
            break;
        case BONUS_TOTAL_STAGE_WAIT:
            if (!Frame_DelayReady(30)) {
                return false;
            }
            g_BonusTotal.phase = BONUS_TOTAL_PREPARE_METRIC;
            break;
        case BONUS_TOTAL_PREPARE_METRIC:
            M_Prepare(host_tick);
            break;
        case BONUS_TOTAL_DIGIT: {
            const int16_t target = g_BonusTotal.digits[g_BonusTotal.place] % 10;
            if (g_BonusTotal.spin < 20) {
                M_PutDigit(assets, g_BonusTotal.spin);
            }
            if (g_BonusTotal.spin == 20
                || (target >= 0 && g_BonusTotal.spin == 10 + target)) {
                M_PutDigit(assets, g_BonusTotal.digits[g_BonusTotal.place]);
                g_BonusTotal.spin = 0;
                g_BonusTotal.place++;
                if (g_BonusTotal.place == g_BonusTotal.digit_count) {
                    g_BonusTotal.phase = BONUS_TOTAL_METRIC_WAIT;
                }
            } else {
                g_BonusTotal.phase = BONUS_TOTAL_DIGIT_WAIT;
            }
            break;
        }
        case BONUS_TOTAL_DIGIT_WAIT: {
            ASSERT(host_tick >= g_BonusTotal.last_tick);
            const uint64_t elapsed = host_tick - g_BonusTotal.last_tick;
            if (elapsed != 0) {
                g_BonusTotal.credit = elapsed >= 100
                    ? 5000
                    : g_BonusTotal.credit + (uint32_t)elapsed * 50;
                g_BonusTotal.last_tick = host_tick;
            }
            if (g_BonusTotal.credit < 12) {
                return false;
            }
            g_BonusTotal.credit -= 12;
            Sound_Effect(2);
            g_BonusTotal.spin++;
            g_BonusTotal.phase = BONUS_TOTAL_DIGIT;
            break;
        }
        case BONUS_TOTAL_METRIC_WAIT:
            if (!Frame_DelayReady(5)) {
                return false;
            }
            g_BonusTotal.metric++;
            if (g_BonusTotal.metric < 5) {
                g_BonusTotal.phase = BONUS_TOTAL_PREPARE_METRIC;
                break;
            }
            GameInput_ResetGame();
            g_GameInput.shot = true;
            g_GameInput.ok = true;
            g_Session.continues_per_scene[g_BonusTotal.stage_number / 5 - 1] =
                (uint16_t)g_Session.continues_total;
            g_BonusTotal.phase = BONUS_TOTAL_CONFIRM;
            break;
        case BONUS_TOTAL_CONFIRM:
            if (GameInput_SenseGame(raw, false)) {
                g_Player.is_hit = true;
            }
            if (g_GameInput.shot && g_GameInput.ok) {
                return false;
            }
            g_Bonus.score_bonus = 0;
            g_BonusTotal.phase = BONUS_TOTAL_DONE;
            return true;
        }
    }
}
