#include "th01/bonus.h"

#include "pc98/zun/math16.h"

#include "core/base/assert.h"

#include <string.h>

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
    ASSERT(Result_Ignore(GameText_Draw(
        c->font, c->display->video, text, 48 + left, 80 + row * 16, color,
        &m_Style)));
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

static void M_Confirm(BONUS *const b, const BONUS_CONTEXT *const c)
{
    GameInput_ResetGame(c->input);
    c->input->shot = true;
    c->input->ok = true;
    b->phase = BONUS_CONFIRM;
}

static void M_StageCommit(BONUS *const b, const BONUS_CONTEXT *const c)
{
    char title[80] = "ＳＴＡＧＥ　";
    const int16_t stage_digits[] = { (int16_t)(b->stage_number / 10),
                                     (int16_t)(b->stage_number % 10) };
    char number[7];
    M_Digits(number, stage_digits, 2);
    strcat(title, number);
    strcat(title, "　ＣＯＭＰＬＥＴＥ");
    M_Text(c, title, 16, 0, 10);
    const int16_t rows[] = { 2, 3, 5, 7, 9, 11, 13 };
    for (int32_t i = 0; i < 7; i++) {
        M_Text(c, m_Labels[i], i == 6 ? 112 : 0, rows[i], i == 6 ? 10 : 7);
    }
    M_StageMetric(c, 3, Math16_Wrap(c->timer->time * 3));
    M_StageMetric(c, 5, Math16_Wrap(c->display->gameplay->maximum_combo * 100));
    M_StageMetric(
        c, 7,
        Math16_Wrap(
            *c->display->remaining_lives * 200
            + c->display->player->remaining_bombs * 100));
    M_StageMetric(c, 9, Math16_Wrap((b->stage_number - 1) * 200));
    c->display->gameplay->score += *c->score_bonus;
    const uint32_t bits = *c->score_bonus;
    c->bonus_per_stage[(b->stage_number - 1) % 5] = bits <= INT32_MAX
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
    Hud_ScoreAndCombo(c->hud, c->display);
    Extend_UpdateAndRender(c->extend);
    M_Confirm(b, c);
}

RESULT Bonus_LoadStageAssets(
    READER *const reader, uint8_t *const scratch, const size_t scratch_size,
    const ALLOCATOR *const allocator, BONUS_STAGE_ASSETS **const out)
{
    *out = nullptr;
    READER image;
    MUST(ReiidenImage_Open(reader, scratch, scratch_size, &image));
    BONUS_STAGE_ASSETS loaded;
    Reader_Seek(&image, 0x38f3a);
    Reader_ReadBytes(&image, loaded.box, sizeof(loaded.box));
    MUST(Reader_Result(&image));
    *out = Allocator_Alloc(allocator, sizeof(loaded));
    **out = loaded;
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
    BONUS *const b, const BONUS_CONTEXT *const c, const int16_t stage_number,
    const TH01_INPUT *const raw, FRAME_VSYNC *const vsync)
{
    ASSERT(
        b->phase == BONUS_DONE && stage_number >= 1 && stage_number <= 20
        && stage_number % 5 != 0);
    *b = (BONUS) { .phase = BONUS_STAGE_BOX, .stage_number = stage_number };
    Palette_Set(c->palette, c->display->video, c->stage_palette);
    return Bonus_Resume(b, c, raw, vsync);
}

bool Bonus_Resume(
    BONUS *const b, const BONUS_CONTEXT *const c, const TH01_INPUT *const raw,
    FRAME_VSYNC *const vsync)
{
    for (;;) {
        switch (b->phase) {
        case BONUS_DONE:
            return true;
        case BONUS_STAGE_BOX:
            if (b->waiting) {
                if (!Frame_DelayReady(vsync, 1)) {
                    return false;
                }
                b->waiting = false;
            }
            if (b->row == M_PADDED_HEIGHT / 2) {
                M_StageCommit(b, c);
                break;
            }
            for (int32_t x = 32; x < 32 + M_STAGE_WIDTH + 32; x++) {
                if ((c->stage->box[b->row % 4] & (0x80u >> (x % 8))) != 0) {
                    Video_Pixel(c->display->video, x, 180 - b->row, 0);
                    Video_Pixel(c->display->video, x, 180 + b->row, 0);
                }
            }
            b->waiting = b->row % 6 == 0;
            b->row++;
            break;
        case BONUS_CONFIRM:
            if (GameInput_SenseGame(c->input, raw, false)) {
                c->display->player->is_hit = true;
            }
            if (c->input->shot && c->input->ok) {
                return false;
            }
            *c->score_bonus = 0;
            b->phase = BONUS_DONE;
            return true;
        }
    }
}
