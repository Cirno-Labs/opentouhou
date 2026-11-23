#include "th01/bonus.h"

#include "pc98/zun/math16.h"
#include "th01/sprite.h"

#include "core/base/assert.h"

static void M_PutDigit(
    const BONUS_TOTAL *const b, const BONUS_TOTAL_CONTEXT *const c,
    const int16_t digit)
{
    const int16_t left =
        (int16_t)(224 + (b->place - (b->digit_count == 6)) * 16);
    Sprite_Restore16(left, b->top, 16, 16);
    if (digit >= 0 && digit <= 19) {
        Sprite_PtnPut(
            &c->assets->numerals, 10 + (digit % 10) / 4, digit % 4, left,
            b->top);
    }
}

static int32_t M_Add(
    const BONUS_CONTEXT *const c, int32_t value, const int32_t cap)
{
    if (value > cap) {
        value = cap;
    }
    for (int32_t i = 0; i < 10; i++) {
        *c->score_bonus += (uint32_t)value;
    }
    return value;
}

static void M_Prepare(
    BONUS_TOTAL *const b, const BONUS_TOTAL_CONTEXT *const tc,
    const uint64_t host_tick)
{
    const BONUS_CONTEXT *const c = tc->common;
    int32_t value;
    if (b->metric == 4) {
        c->display->gameplay->score += *c->score_bonus;
        const int16_t div = Math16_Wrap((int32_t)(*c->score_bonus / 10000));
        const int16_t rem = (int16_t)(*c->score_bonus % 10000);
        b->digits[0] = (int16_t)(div / 10);
        b->digits[1] = (int16_t)(div % 10);
        b->digits[2] = (int16_t)(rem / 1000);
        b->digits[3] = (int16_t)(rem % 1000 / 100);
        b->digits[4] = (int16_t)(rem % 100 / 10);
        b->digits[5] = (int16_t)(rem % 10);
        b->top = 196;
        b->digit_count = 6;
    } else {
        switch (b->metric) {
        case 0:
            value = (uint16_t)(g_StageTimer.time * 5);
            break;
        case 1:
            value = Math16_Wrap(c->display->gameplay->maximum_combo * 200);
            break;
        case 2:
            value = Math16_Wrap(*c->display->remaining_lives * 500);
            value += Math16_Wrap(c->display->player->remaining_bombs * 200);
            break;
        default:
            value = Math16_Wrap(b->stage_number * 1000);
            break;
        }
        value = M_Add(c, value, b->metric == 3 ? 65530 : 6553);
        b->top = (int16_t)(86 + b->metric * 21);
        b->digit_count = b->metric == 3 ? 6 : 5;
        int32_t divisor = b->metric == 3 ? 10000 : 1000;
        for (int32_t i = 0; i < b->digit_count; i++) {
            if (divisor == 0) {
                b->digits[i] = 0;
            } else {
                b->digits[i] = (int16_t)(value / divisor);
                value %= divisor;
                divisor /= 10;
            }
        }
        if (b->metric == 3) {
            b->digits[4] = 0;
        }
    }
    b->place = 0;
    b->spin = 0;
    if (b->metric == 0) {
        b->credit = 49;
        b->last_tick = host_tick;
    }
    b->phase = BONUS_TOTAL_DIGIT;
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
    BONUS_TOTAL *const b, const BONUS_TOTAL_CONTEXT *const c,
    const int16_t stage_number, const TH01_INPUT *const raw,
    const uint64_t host_tick)
{
    ASSERT(
        b->phase == BONUS_TOTAL_DONE && stage_number >= 5 && stage_number <= 20
        && stage_number % 5 == 0);
    *b = (BONUS_TOTAL) { .phase = BONUS_TOTAL_INITIAL_WAIT,
                         .stage_number = stage_number,
                         .last_tick = host_tick };
    Shots_EraseAndReset(c->shots, c->shots_output);
    Pellets_EraseAndResetMoving(c->pellets);
    return Bonus_ResumeTotal(b, c, raw, host_tick);
}

bool Bonus_ResumeTotal(
    BONUS_TOTAL *const b, const BONUS_TOTAL_CONTEXT *const c,
    const TH01_INPUT *const raw, const uint64_t host_tick)
{
    for (;;) {
        switch (b->phase) {
        case BONUS_TOTAL_DONE:
            return true;
        case BONUS_TOTAL_INITIAL_WAIT: {
            if (!Frame_DelayReady(50)) {
                return false;
            }
            const VIDEO_IMAGE image = { .pixels = c->assets->background.pixels,
                                        .width = VIDEO_WIDTH,
                                        .height = VIDEO_HEIGHT,
                                        .stride = VIDEO_WIDTH,
                                        .key = -1 };
            Video_SelectDrawingPage(1);
            Video_Blit(
                &image, (GFX_RECT) { 0, 0, VIDEO_WIDTH, VIDEO_HEIGHT }, 0, 0);
            c->common->palette->resource = c->assets->palette;
            Video_SelectDrawingPage(0);
            Bonus_BeginRowTransition(&b->transition);
            b->phase = BONUS_TOTAL_TRANSITION;
            break;
        }
        case BONUS_TOTAL_TRANSITION:
            if (!Bonus_ResumeRowTransition(
                    &b->transition, c->common->palette,
                    &c->common->palette->resource)) {
                return false;
            }
            Sprite_PtnPut(
                &c->assets->numerals, b->stage_number / 10, -1, 224, 14);
            Sprite_PtnPut(
                &c->assets->numerals, b->stage_number % 10, -1, 256, 14);
            b->phase = BONUS_TOTAL_STAGE_WAIT;
            break;
        case BONUS_TOTAL_STAGE_WAIT:
            if (!Frame_DelayReady(30)) {
                return false;
            }
            b->phase = BONUS_TOTAL_PREPARE_METRIC;
            break;
        case BONUS_TOTAL_PREPARE_METRIC:
            M_Prepare(b, c, host_tick);
            break;
        case BONUS_TOTAL_DIGIT: {
            const int16_t target = b->digits[b->place] % 10;
            if (b->spin < 20) {
                M_PutDigit(b, c, b->spin);
            }
            if (b->spin == 20 || (target >= 0 && b->spin == 10 + target)) {
                M_PutDigit(b, c, b->digits[b->place]);
                b->spin = 0;
                b->place++;
                if (b->place == b->digit_count) {
                    b->phase = BONUS_TOTAL_METRIC_WAIT;
                }
            } else {
                b->phase = BONUS_TOTAL_DIGIT_WAIT;
            }
            break;
        }
        case BONUS_TOTAL_DIGIT_WAIT: {
            ASSERT(host_tick >= b->last_tick);
            const uint64_t elapsed = host_tick - b->last_tick;
            if (elapsed != 0) {
                b->credit =
                    elapsed >= 100 ? 5000 : b->credit + (uint32_t)elapsed * 50;
                b->last_tick = host_tick;
            }
            if (b->credit < 12) {
                return false;
            }
            b->credit -= 12;
            c->effect(c->user, 2);
            b->spin++;
            b->phase = BONUS_TOTAL_DIGIT;
            break;
        }
        case BONUS_TOTAL_METRIC_WAIT:
            if (!Frame_DelayReady(5)) {
                return false;
            }
            b->metric++;
            if (b->metric < 5) {
                b->phase = BONUS_TOTAL_PREPARE_METRIC;
                break;
            }
            GameInput_ResetGame();
            g_GameInput.shot = true;
            g_GameInput.ok = true;
            c->continues_per_scene[b->stage_number / 5 - 1] =
                (uint16_t)*c->continues_total;
            b->phase = BONUS_TOTAL_CONFIRM;
            break;
        case BONUS_TOTAL_CONFIRM:
            if (GameInput_SenseGame(raw, false)) {
                c->common->display->player->is_hit = true;
            }
            if (g_GameInput.shot && g_GameInput.ok) {
                return false;
            }
            *c->common->score_bonus = 0;
            b->phase = BONUS_TOTAL_DONE;
            return true;
        }
    }
}
