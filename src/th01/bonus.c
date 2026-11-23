#include "th01/bonus.h"
#include "th01/session.h"

#include "pc98/zun/math16.h"
#include "pc98/zun/sprite_sheet.h"

#include "core/base/assert.h"

#include <string.h>

BONUS g_Bonus;

#define M_STAGE_WIDTH 288
#define M_PADDED_HEIGHT 280

static const char *const m_Labels[] = {
    "　　ＢＯＮＵＳ",         "   　Ｔｉｍｅ　", "Ｃｏｎｔｉｎｕｏｕｓ",
    "Ｂｏｍｂ＆Ｐｌａｙｅｒ", "　　ＳＴＡＧＥ",  "ＢＯＮＵＳ　Ｐｏｉｎｔ",
    "Ｈｉｔ　Ｚ　Ｋｅｙ"
};
static const char m_Characters[] =
    " ＳＴＡＧＥ　ＣＯＭＰＬＥＴＥＢＯＮＵＳＴｉｍｅＣｏｎｔｉｎｕｏｕｓ"
    "Ｂｏｍｂ＆ＰｌａｙｅｒＰｏｉｎｔＨｉｔＺＫｅｙ０１２３４５６７８９";
static const GAME_TEXT_STYLE m_Style = { .weight = GAME_TEXT_BOLD };

static void M_Digits(
    char *const out, const int16_t *const digits, const int32_t count)
{
    for (int32_t i = 0; i < count; i++) {
        const uint32_t cp = digits[i] >= 0 && digits[i] <= 9
            ? (uint32_t)(0xff10 + digits[i])
            : 0x3000;
        out[i * 3] = (char)(0xe0 | (cp >> 12));
        out[i * 3 + 1] = (char)(0x80 | ((cp >> 6) & 63));
        out[i * 3 + 2] = (char)(0x80 | (cp & 63));
    }
    out[count * 3] = '\0';
}

static void M_Text(
    const BONUS_CONTEXT *const c, const char *const text, const int16_t left,
    const int16_t row, const uint8_t color)
{
    GameText_Draw(c->font, text, 48 + left, 80 + row * 16, color, &m_Style);
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

static void M_StageMetric(
    const BONUS_CONTEXT *const c, const int16_t row, const int16_t value)
{
    const int16_t v = (int16_t)M_Add(c, value, 6553);
    const int16_t digits[] = { (int16_t)(v / 1000),
                               (int16_t)(v % 1000 / 100),
                               (int16_t)(v % 100 / 10),
                               (int16_t)(v % 10),
                               0,
                               10 };
    char text[19];
    M_Digits(text, digits, 6);
    M_Text(c, text, 208, row, 7);
}

static void M_Confirm(const BONUS_CONTEXT *const c)
{
    GameInput_ResetGame();
    g_GameInput.shot = true;
    g_GameInput.ok = true;
    g_Bonus.phase = BONUS_CONFIRM;
}

static void M_StageCommit(const BONUS_CONTEXT *const c)
{
    char title[80] = "ＳＴＡＧＥ　";
    const int16_t stage_digits[] = { (int16_t)(g_Bonus.stage_number / 10),
                                     (int16_t)(g_Bonus.stage_number % 10) };
    char number[7];
    M_Digits(number, stage_digits, 2);
    strcat(title, number);
    strcat(title, "　ＣＯＭＰＬＥＴＥ");
    M_Text(c, title, 16, 0, 10);
    const int16_t rows[] = { 2, 3, 5, 7, 9, 11, 13 };
    for (int32_t i = 0; i < 7; i++) {
        M_Text(c, m_Labels[i], i == 6 ? 112 : 0, rows[i], i == 6 ? 10 : 7);
    }
    M_StageMetric(c, 3, Math16_Wrap(g_StageTimer.time * 3));
    M_StageMetric(c, 5, Math16_Wrap(g_Stage.maximum_combo * 100));
    M_StageMetric(
        c, 7,
        Math16_Wrap(g_Session.lives * 200 + g_Player.remaining_bombs * 100));
    M_StageMetric(c, 9, Math16_Wrap((g_Bonus.stage_number - 1) * 200));
    g_Stage.score += *c->score_bonus;
    const uint32_t bits = *c->score_bonus;
    c->bonus_per_stage[(g_Bonus.stage_number - 1) % 5] = bits <= INT32_MAX
        ? (int32_t)bits
        : (int32_t)((int64_t)bits - INT64_C(4294967296));
    const int16_t div = Math16_Wrap((int32_t)(bits / 10000));
    const int16_t remainder = Math16_Wrap((int32_t)(bits % 10000));
    const int16_t digits[] = { (int16_t)(div / 10),
                               (int16_t)(div % 10),
                               (int16_t)(remainder / 1000),
                               (int16_t)(remainder % 1000 / 100),
                               (int16_t)(remainder % 100 / 10),
                               0 };
    char text[19];
    M_Digits(text, digits, 6);
    M_Text(c, text, 192, 11, 7);
    Hud_ScoreAndCombo();
    Extend_UpdateAndRender();
    M_Confirm(c);
}

RESULT Bonus_LoadStageAssets(
    VFS *const vfs, const ALLOCATOR *const allocator,
    BONUS_STAGE_ASSETS **const out)
{
    *out = nullptr;
    uint8_t *box;
    size_t size;
    MUST(SpriteSheet_Load(vfs, "bonusbox.bmp", 8, 4, allocator, &box, &size));
    FAIL_IF(size != sizeof(BONUS_STAGE_ASSETS), "Invalid bonusbox.bmp size");
    *out = (BONUS_STAGE_ASSETS *)box;
    return OK;
}

RESULT Bonus_LoadFont(
    const FONT_ROM *const rom, const ALLOCATOR *const allocator,
    GAME_TEXT_FONT *const out)
{
    return GameText_Load(rom, m_Characters, allocator, out);
}

RESULT Bonus_ValidateStageAssets(
    const BONUS_STAGE_ASSETS *const assets, const GAME_TEXT_FONT *const font)
{
    FAIL_IF(assets == nullptr || font == nullptr, "Missing stage bonus assets");
    for (const uint8_t *p = (const uint8_t *)m_Characters; *p != 0;) {
        uint32_t cp;
        if (*p < 128) {
            cp = *p++;
        } else {
            cp = ((uint32_t)(p[0] & 15) << 12) | ((uint32_t)(p[1] & 63) << 6)
                | (p[2] & 63);
            p += 3;
        }
        const GAME_TEXT_GLYPH *const glyph = GameText_Find(font, cp);
        FAIL_IF(
            glyph == nullptr || glyph->width != (cp < 128 ? 8 : 16),
            "Missing stage bonus glyph");
    }
    return OK;
}

bool Bonus_BeginStage(
    const BONUS_CONTEXT *const c, const int16_t stage_number,
    const TH01_INPUT *const raw)
{
    ASSERT(
        g_Bonus.phase == BONUS_DONE && stage_number >= 1 && stage_number <= 20
        && stage_number % 5 != 0);
    memset(&g_Bonus, 0, sizeof(g_Bonus));
    g_Bonus.phase = BONUS_STAGE_BOX;
    g_Bonus.stage_number = stage_number;
    Palette_Set(&g_Palette, &g_StagePalette);
    return Bonus_Resume(c, raw);
}

bool Bonus_Resume(const BONUS_CONTEXT *const c, const TH01_INPUT *const raw)
{
    for (;;) {
        switch (g_Bonus.phase) {
        case BONUS_DONE:
            return true;
        case BONUS_STAGE_BOX:
            if (g_Bonus.waiting) {
                if (!Frame_DelayReady(1)) {
                    return false;
                }
                g_Bonus.waiting = false;
            }
            if (g_Bonus.row == M_PADDED_HEIGHT / 2) {
                M_StageCommit(c);
                break;
            }
            for (int32_t x = 32; x < 32 + M_STAGE_WIDTH + 32; x++) {
                if ((c->stage->box[g_Bonus.row % 4] & (0x80u >> (x % 8)))
                    != 0) {
                    Video_Pixel(x, 180 - g_Bonus.row, 0);
                    Video_Pixel(x, 180 + g_Bonus.row, 0);
                }
            }
            g_Bonus.waiting = g_Bonus.row % 6 == 0;
            g_Bonus.row++;
            break;
        case BONUS_CONFIRM:
            if (GameInput_SenseGame(raw, false)) {
                g_Player.is_hit = true;
            }
            if (g_GameInput.shot && g_GameInput.ok) {
                return false;
            }
            *c->score_bonus = 0;
            g_Bonus.phase = BONUS_DONE;
            return true;
        }
    }
}
