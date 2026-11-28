#include "th01/bonus/bonus.h"

#include "th01/session.h"
#include "th01/sound.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"
#include "pc98/zun/sprite_sheet.h"

#include <string.h>

#define M_STAGE_WIDTH 288
#define M_PADDED_HEIGHT 280

typedef enum {
    BONUS_DONE,
    BONUS_STAGE_BOX,
    BONUS_CONFIRM,
} BONUS_PHASE;

typedef struct {
    BONUS_PHASE phase;
    int16_t stage_number;
    int16_t row;
    bool waiting;
} BONUS_RUN;

typedef struct {
    BONUS_RUN run;
    // Accumulates across the stage bonuses leading up to a total; Bonus_Resume
    // and Bonus_ResumeTotal clear it on confirmation. Bonus_BeginStage resets
    // only run, so this survives between stage bonus runs.
    uint32_t score_bonus;
} BONUS;

typedef enum {
    BONUS_TOTAL_DONE,
    BONUS_TOTAL_INITIAL_WAIT,
    BONUS_TOTAL_TRANSITION,
    BONUS_TOTAL_STAGE_WAIT,
    BONUS_TOTAL_PREPARE_METRIC,
    BONUS_TOTAL_DIGIT,
    BONUS_TOTAL_DIGIT_WAIT,
    BONUS_TOTAL_METRIC_WAIT,
    BONUS_TOTAL_CONFIRM,
} BONUS_TOTAL_PHASE;

typedef struct {
    BONUS_TOTAL_PHASE phase;
    uint64_t last_tick;
    uint32_t credit;
    int16_t stage_number;
    int16_t digits[6];
    int16_t top;
    int16_t metric;
    int16_t place;
    int16_t digit_count;
    int16_t spin;
} BONUS_TOTAL;

static BONUS m_Bonus;
static BONUS_TOTAL m_Total;

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
    const GAME_TEXT_FONT *const font, const char *const text,
    const int16_t left, const int16_t row, const uint8_t color)
{
    GameText_Draw(font, text, 48 + left, 80 + row * 16, color, &m_Style);
}

static int32_t M_Add(int32_t value, const int32_t cap)
{
    if (value > cap) {
        value = cap;
    }
    for (int32_t i = 0; i < 10; i++) {
        m_Bonus.score_bonus += (uint32_t)value;
    }
    return value;
}

static void M_StageMetric(
    const GAME_TEXT_FONT *const font, const int16_t row, const int16_t value)
{
    const int16_t v = (int16_t)M_Add(value, 6553);
    const int16_t digits[] = { (int16_t)(v / 1000),
                               (int16_t)(v % 1000 / 100),
                               (int16_t)(v % 100 / 10),
                               (int16_t)(v % 10),
                               0,
                               10 };
    char text[19];
    M_Digits(text, digits, 6);
    M_Text(font, text, 208, row, 7);
}

static void M_Confirm(void)
{
    GameInput_ResetGame();
    GameInput_HoldConfirm();
    m_Bonus.run.phase = BONUS_CONFIRM;
}

static void M_StageCommit(const GAME_TEXT_FONT *const font)
{
    char title[80] = "ＳＴＡＧＥ　";
    const int16_t stage_digits[] = { (int16_t)(m_Bonus.run.stage_number / 10),
                                     (int16_t)(m_Bonus.run.stage_number % 10) };
    char number[7];
    M_Digits(number, stage_digits, 2);
    strcat(title, number);
    strcat(title, "　ＣＯＭＰＬＥＴＥ");
    M_Text(font, title, 16, 0, 10);
    const int16_t rows[] = { 2, 3, 5, 7, 9, 11, 13 };
    for (int32_t i = 0; i < 7; i++) {
        M_Text(font, m_Labels[i], i == 6 ? 112 : 0, rows[i], i == 6 ? 10 : 7);
    }
    M_StageMetric(font, 3, Math16_Wrap(g_Stage->time * 3));
    M_StageMetric(font, 5, Math16_Wrap(g_Stage->maximum_combo * 100));
    M_StageMetric(
        font, 7,
        Math16_Wrap(g_Session.lives * 200 + g_Player->remaining_bombs * 100));
    M_StageMetric(font, 9, Math16_Wrap((m_Bonus.run.stage_number - 1) * 200));
    Stage_AddScore(m_Bonus.score_bonus);
    const uint32_t bits = m_Bonus.score_bonus;
    g_Session.bonus_per_stage[(m_Bonus.run.stage_number - 1) % 5] =
        bits <= INT32_MAX ? (int32_t)bits
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
    M_Text(font, text, 192, 11, 7);
    Hud_ScoreAndCombo();
    Extend_UpdateAndRender();
    M_Confirm();
}

static void M_PutDigit(
    const BONUS_TOTAL_ASSETS *const assets, const int16_t digit)
{
    const int16_t left =
        (int16_t)(224 + (m_Total.place - (m_Total.digit_count == 6)) * 16);
    Sprite_Restore16(left, m_Total.top, 16, 16);
    if (digit >= 0 && digit <= 19) {
        Sprite_PtnPut(
            &assets->numerals, 10 + (digit % 10) / 4, digit % 4, left,
            m_Total.top);
    }
}

static void M_Prepare(const uint64_t host_tick)
{
    int32_t value;
    if (m_Total.metric == 4) {
        Stage_AddScore(m_Bonus.score_bonus);
        const int16_t div = Math16_Wrap((int32_t)(m_Bonus.score_bonus / 10000));
        const int16_t rem = (int16_t)(m_Bonus.score_bonus % 10000);
        m_Total.digits[0] = (int16_t)(div / 10);
        m_Total.digits[1] = (int16_t)(div % 10);
        m_Total.digits[2] = (int16_t)(rem / 1000);
        m_Total.digits[3] = (int16_t)(rem % 1000 / 100);
        m_Total.digits[4] = (int16_t)(rem % 100 / 10);
        m_Total.digits[5] = (int16_t)(rem % 10);
        m_Total.top = 196;
        m_Total.digit_count = 6;
    } else {
        switch (m_Total.metric) {
        case 0:
            value = (uint16_t)(g_Stage->time * 5);
            break;
        case 1:
            value = Math16_Wrap(g_Stage->maximum_combo * 200);
            break;
        case 2:
            value = Math16_Wrap(g_Session.lives * 500);
            value += Math16_Wrap(g_Player->remaining_bombs * 200);
            break;
        default:
            value = Math16_Wrap(m_Total.stage_number * 1000);
            break;
        }
        value = M_Add(value, m_Total.metric == 3 ? 65530 : 6553);
        m_Total.top = (int16_t)(86 + m_Total.metric * 21);
        m_Total.digit_count = m_Total.metric == 3 ? 6 : 5;
        int32_t divisor = m_Total.metric == 3 ? 10000 : 1000;
        for (int32_t i = 0; i < m_Total.digit_count; i++) {
            if (divisor == 0) {
                m_Total.digits[i] = 0;
            } else {
                m_Total.digits[i] = (int16_t)(value / divisor);
                value %= divisor;
                divisor /= 10;
            }
        }
        if (m_Total.metric == 3) {
            m_Total.digits[4] = 0;
        }
    }
    m_Total.place = 0;
    m_Total.spin = 0;
    if (m_Total.metric == 0) {
        m_Total.credit = 49;
        m_Total.last_tick = host_tick;
    }
    m_Total.phase = BONUS_TOTAL_DIGIT;
}

void Bonus_Init(void)
{
    memset(&m_Bonus, 0, sizeof(m_Bonus));
    Bonus_InitTotal();
}

RESULT BonusAssets_LoadStage(
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

RESULT BonusAssets_LoadFont(
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
    const BONUS_STAGE_ASSETS *const stage, const GAME_TEXT_FONT *const font,
    const int16_t stage_number, const TH01_INPUT *const raw)
{
    ASSERT(
        m_Bonus.run.phase == BONUS_DONE && stage_number >= 1
        && stage_number <= 20 && stage_number % 5 != 0);
    memset(&m_Bonus.run, 0, sizeof(m_Bonus.run));
    m_Bonus.run.phase = BONUS_STAGE_BOX;
    m_Bonus.run.stage_number = stage_number;
    Palette_SetGlobal(g_StagePalette);
    return Bonus_Resume(stage, font, raw);
}

bool Bonus_Resume(
    const BONUS_STAGE_ASSETS *const stage, const GAME_TEXT_FONT *const font,
    const TH01_INPUT *const raw)
{
    for (;;) {
        switch (m_Bonus.run.phase) {
        case BONUS_DONE:
            return true;
        case BONUS_STAGE_BOX:
            if (m_Bonus.run.waiting) {
                if (!VSyncIrq_DelayReady(1)) {
                    return false;
                }
                m_Bonus.run.waiting = false;
            }
            if (m_Bonus.run.row == M_PADDED_HEIGHT / 2) {
                M_StageCommit(font);
                break;
            }
            for (int32_t x = 32; x < 32 + M_STAGE_WIDTH + 32; x++) {
                if ((stage->box[m_Bonus.run.row % 4] & (0x80u >> (x % 8)))
                    != 0) {
                    Video_Pixel(x, 180 - m_Bonus.run.row, 0);
                    Video_Pixel(x, 180 + m_Bonus.run.row, 0);
                }
            }
            m_Bonus.run.waiting = m_Bonus.run.row % 6 == 0;
            m_Bonus.run.row++;
            break;
        case BONUS_CONFIRM:
            if (GameInput_SenseGame(raw, false)) {
                Player_Hit();
            }
            if (g_GameInput->shot && g_GameInput->ok) {
                return false;
            }
            m_Bonus.score_bonus = 0;
            m_Bonus.run.phase = BONUS_DONE;
            return true;
        }
    }
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

RESULT BonusAssets_LoadTotal(
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

void Bonus_InitTotal(void)
{
    memset(&m_Total, 0, sizeof(m_Total));
    Bonus_InitRowTransition();
}

bool Bonus_BeginTotal(
    const BONUS_TOTAL_ASSETS *const assets, const int16_t stage_number,
    const TH01_INPUT *const raw, const uint64_t host_tick)
{
    ASSERT(
        m_Total.phase == BONUS_TOTAL_DONE && stage_number >= 5
        && stage_number <= 20 && stage_number % 5 == 0);
    Bonus_InitTotal();
    m_Total.phase = BONUS_TOTAL_INITIAL_WAIT;
    m_Total.stage_number = stage_number;
    m_Total.last_tick = host_tick;
    Shots_EraseAndReset();
    Pellets_EraseAndResetMoving();
    return Bonus_ResumeTotal(assets, raw, host_tick);
}

bool Bonus_ResumeTotal(
    const BONUS_TOTAL_ASSETS *const assets, const TH01_INPUT *const raw,
    const uint64_t host_tick)
{
    for (;;) {
        switch (m_Total.phase) {
        case BONUS_TOTAL_DONE:
            return true;
        case BONUS_TOTAL_INITIAL_WAIT: {
            if (!VSyncIrq_DelayReady(50)) {
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
            Palette_SetResource(&assets->palette);
            Video_SelectDrawingPage(0);
            Bonus_BeginRowTransition();
            m_Total.phase = BONUS_TOTAL_TRANSITION;
            break;
        }
        case BONUS_TOTAL_TRANSITION:
            if (!Bonus_ResumeRowTransition(&g_Palette->resource)) {
                return false;
            }
            Sprite_PtnPut(
                &assets->numerals, m_Total.stage_number / 10, -1, 224, 14);
            Sprite_PtnPut(
                &assets->numerals, m_Total.stage_number % 10, -1, 256, 14);
            m_Total.phase = BONUS_TOTAL_STAGE_WAIT;
            break;
        case BONUS_TOTAL_STAGE_WAIT:
            if (!VSyncIrq_DelayReady(30)) {
                return false;
            }
            m_Total.phase = BONUS_TOTAL_PREPARE_METRIC;
            break;
        case BONUS_TOTAL_PREPARE_METRIC:
            M_Prepare(host_tick);
            break;
        case BONUS_TOTAL_DIGIT: {
            const int16_t target = m_Total.digits[m_Total.place] % 10;
            if (m_Total.spin < 20) {
                M_PutDigit(assets, m_Total.spin);
            }
            if (m_Total.spin == 20
                || (target >= 0 && m_Total.spin == 10 + target)) {
                M_PutDigit(assets, m_Total.digits[m_Total.place]);
                m_Total.spin = 0;
                m_Total.place++;
                if (m_Total.place == m_Total.digit_count) {
                    m_Total.phase = BONUS_TOTAL_METRIC_WAIT;
                }
            } else {
                m_Total.phase = BONUS_TOTAL_DIGIT_WAIT;
            }
            break;
        }
        case BONUS_TOTAL_DIGIT_WAIT: {
            ASSERT(host_tick >= m_Total.last_tick);
            const uint64_t elapsed = host_tick - m_Total.last_tick;
            if (elapsed != 0) {
                m_Total.credit = elapsed >= 100
                    ? 5000
                    : m_Total.credit + (uint32_t)elapsed * 50;
                m_Total.last_tick = host_tick;
            }
            if (m_Total.credit < 12) {
                return false;
            }
            m_Total.credit -= 12;
            Sound_Effect(SOUND_EFFECT_HIT);
            m_Total.spin++;
            m_Total.phase = BONUS_TOTAL_DIGIT;
            break;
        }
        case BONUS_TOTAL_METRIC_WAIT:
            if (!VSyncIrq_DelayReady(5)) {
                return false;
            }
            m_Total.metric++;
            if (m_Total.metric < 5) {
                m_Total.phase = BONUS_TOTAL_PREPARE_METRIC;
                break;
            }
            GameInput_ResetGame();
            GameInput_HoldConfirm();
            g_Session.continues_per_scene[m_Total.stage_number / 5 - 1] =
                (uint16_t)g_Session.continues_total;
            m_Total.phase = BONUS_TOTAL_CONFIRM;
            break;
        case BONUS_TOTAL_CONFIRM:
            if (GameInput_SenseGame(raw, false)) {
                Player_Hit();
            }
            if (g_GameInput->shot && g_GameInput->ok) {
                return false;
            }
            m_Bonus.score_bonus = 0;
            m_Total.phase = BONUS_TOTAL_DONE;
            return true;
        }
    }
}
