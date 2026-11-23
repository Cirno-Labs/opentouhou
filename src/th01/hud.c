#include "th01/hud.h"

#include "th01/sprite.h"

#include "core/base/assert.h"

static const char m_Characters[] =
    " !\"#$%&'()*+,-./"
    "0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
    "abcdefghijklmnopqrstuvwxyz{|}~"
    "０１２３４５６７８９";
static const char *const m_Ranks[] = { "EASY", "NORMAL", "HARD", "LUNATIC" };
static const char *const m_Numerals[] = { "０", "１", "２", "３", "４",
                                          "５", "６", "７", "８", "９" };

static void M_Text(
    const HUD_CONTEXT *const c, const char *const text, const int32_t left,
    const int32_t top, const uint8_t color, const bool maximum)
{
    const GAME_TEXT_STYLE style = { .weight = maximum ? GAME_TEXT_BOLD
                                                      : GAME_TEXT_BLACK };
    GameText_Draw(c->font, c->video, text, left, top, color, &style);
}

static void M_Snap(
    uint8_t *const pixels, const VIDEO *const video, const int32_t left,
    const int32_t top)
{
    Video_Read(
        video, video->drawing_page, (GFX_RECT) { left, top, 16, 16 }, pixels,
        16);
}

static void M_Put(
    const uint8_t *const pixels, VIDEO *const video, const int32_t left,
    const int32_t top)
{
    const VIDEO_IMAGE image = {
        .pixels = pixels, .width = 16, .height = 16, .stride = 16, .key = -1
    };
    Video_Blit(video, &image, (GFX_RECT) { 0, 0, 16, 16 }, left, top);
}

static void M_Copy(
    VIDEO *const video, const int32_t left, const int32_t top,
    const int32_t width)
{
    Video_Copy(
        video, 0, (GFX_RECT) { left, top, width / 16 * 16, 16 }, 1, left, top);
    Video_SelectDrawingPage(video, 0);
}

static int64_t M_Signed32(const uint32_t value)
{
    return value <= INT32_MAX ? value : (int64_t)value - INT64_C(4294967296);
}

static void M_Number(
    HUD *const hud, const HUD_CONTEXT *const c, const int32_t bank,
    const int32_t count, const uint32_t value, const uint32_t previous,
    const bool signed_current)
{
    const int32_t left = bank < 2 ? 256 : 400;
    const int32_t top = bank % 2 == 0 ? 0 : 16;
    uint8_t *const bg = hud->numbers + bank * 7 * 256;
    uint32_t divisor = count == 7 ? 1000000 : 10;
    {
        for (int32_t i = 0; i < count; i++, divisor /= 10) {
            const int64_t current = signed_current ? M_Signed32(value) : value;
            if (hud->force_numbers
                || current / divisor % 10
                    != M_Signed32(previous) / divisor % 10) {
                for (int32_t page = 1; page >= 0; page--) {
                    Video_SelectDrawingPage(c->video, page);
                    M_Put(bg + i * 256, c->video, left + i * 16, top);
                }
            }
        }
    }
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(c->video, page);
        divisor = count == 7 ? 1000000 : 10;
        const uint32_t text_previous = hud->force_numbers ? 0 : previous;
        for (int32_t i = 0; i < count; i++, divisor /= 10) {
            const uint32_t digit = value / divisor % 10;
            if (text_previous == 0 || digit != text_previous / divisor % 10) {
                M_Text(
                    c, m_Numerals[digit], left + i * 16, top, 7, bank % 2 == 0);
            }
        }
    }
}

static void M_Format(char *const text, uint16_t value, const int32_t width)
{
    uint16_t divisor = width == 4 ? 1000 : 10;
    bool started = false;
    for (int32_t i = 0; i < width; i++, divisor /= 10) {
        started |= value / divisor != 0;
        text[i] = started ? (char)('0' + value / divisor) : ' ';
        value %= divisor;
    }
    text[width] = '\0';
}

static void M_Resource(
    HUD *const hud, const HUD_CONTEXT *const c, const bool bomb,
    const int16_t previous, const bool initial)
{
    const int16_t count =
        bomb ? c->player->remaining_bombs : *c->remaining_lives;
    ASSERT(count >= 0 && count <= HUD_RESOURCE_CAPACITY);
    ASSERT(previous >= 0 && previous <= HUD_RESOURCE_CAPACITY);
    uint8_t *const bg = bomb ? hud->bombs : hud->lives;
    const int32_t row = bomb ? 16 : 0;
    for (int32_t i = initial    ? 0
             : count < previous ? count
                                : previous;
         i < (initial || count >= previous ? count : previous); i++) {
        const int32_t left = 128 + (bomb ? i : i % 6) * 16;
        const int32_t top = bomb ? 16 : i / 6 * 16;
        if (!initial && count < previous) {
            for (int32_t page = 1; page >= 0; page--) {
                Video_SelectDrawingPage(c->video, page);
                M_Put(bg + i * 256, c->video, left, top);
            }
        } else {
            M_Snap(bg + i * 256, c->video, left, top);
            if (!initial) {
                Video_SelectDrawingPage(c->video, 0);
            }
            Sprite_PtnPut(
                c->video, c->sprites, 0, bomb ? 1 : 0, (int16_t)left,
                (int16_t)top);
        }
    }
    if (initial || count > previous) {
        M_Copy(c->video, 128, row, count * 16);
    }
}

RESULT Hud_LoadFont(
    const FONT_ROM *const rom, const ALLOCATOR *const allocator,
    GAME_TEXT_FONT *const out)
{
    return GameText_Load(rom, m_Characters, allocator, out);
}

RESULT Hud_ValidateAssets(
    const GRF *const mask, const PTN *const sprites,
    const GAME_TEXT_FONT *const font)
{
    if (mask->coverage == nullptr || sprites->count < 1
        || sprites->pixels == nullptr) {
        return FAIL("HUD requires a mask and stage icons");
    }
    for (uint32_t cp = 0xff10; cp <= 0xff19; cp++) {
        const GAME_TEXT_GLYPH *const glyph = GameText_Find(font, cp);
        if (glyph == nullptr || glyph->width != 16) {
            return FAIL("HUD requires fullwidth numerals");
        }
    }
    for (const char *p = m_Characters; *p != '\0' && (uint8_t)*p < 128; p++) {
        const GAME_TEXT_GLYPH *const glyph =
            GameText_Find(font, (uint32_t)(uint8_t)*p);
        if (glyph == nullptr || glyph->width != 8) {
            return FAIL("HUD requires ASCII glyphs");
        }
    }
    return OK;
}

void Hud_Init(HUD *const hud, const ALLOCATOR *const allocator)
{
    *hud = (HUD) {};
    uint8_t *const storage =
        Allocator_AllocZ(allocator, (HUD_RESOURCE_CAPACITY * 2 + 28 + 3) * 256);
    hud->lives = storage;
    hud->bombs = storage + HUD_RESOURCE_CAPACITY * 256;
    hud->numbers = hud->bombs + HUD_RESOURCE_CAPACITY * 256;
    hud->stage = hud->numbers + 28 * 256;
    hud->timer = hud->stage + 256;
}

void Hud_PutMask(const HUD_CONTEXT *const c)
{
    const VIDEO_MASK mask = { c->mask->coverage, 640, 66, 640 };
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(c->video, page);
        Video_Mask(c->video, &mask, (GFX_RECT) { 0, 0, 640, 66 }, 0, 0, 10);
    }
}

void Hud_SnapResources(HUD *const hud, const HUD_CONTEXT *const c)
{
    M_Resource(hud, c, false, 0, true);
    M_Resource(hud, c, true, 0, true);
}
void Hud_LivesPut(
    HUD *const hud, const HUD_CONTEXT *const c, const int16_t previous)
{
    M_Resource(hud, c, false, previous, false);
}
void Hud_BombsPut(
    HUD *const hud, const HUD_CONTEXT *const c, const int16_t previous)
{
    M_Resource(hud, c, true, previous, false);
}
void Hud_ScoreAndCombo(HUD *const hud, const HUD_CONTEXT *const c)
{
    const uint32_t score = c->gameplay->score;
    const int16_t combo = c->gameplay->combo;
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(c->video, page);
        for (int32_t bank = 1; bank <= 3; bank += 2) {
            const int32_t count = bank == 1 ? 7 : 2;
            const uint32_t value = bank == 1 ? score : (uint32_t)(int32_t)combo;
            const uint32_t previous = bank == 1
                ? hud->score_previous
                : (uint32_t)(int32_t)hud->combo_previous;
            const int32_t left = bank == 1 ? 256 : 400;
            uint32_t divisor = bank == 1 ? 1000000 : 10;
            for (int32_t i = 0; i < count; i++, divisor /= 10) {
                const int64_t current = bank == 1 ? (int64_t)value : combo;
                if (hud->force_numbers
                    || current / divisor % 10
                        != M_Signed32(previous) / divisor % 10) {
                    M_Put(
                        hud->numbers + (bank * 7 + i) * 256, c->video,
                        left + i * 16, 16);
                }
            }
            divisor = bank == 1 ? 1000000 : 10;
            const uint32_t text_previous = hud->force_numbers ? 0 : previous;
            for (int32_t i = 0; i < count; i++, divisor /= 10) {
                const uint32_t digit = value / divisor % 10;
                if (text_previous == 0
                    || digit != text_previous / divisor % 10) {
                    M_Text(c, m_Numerals[digit], left + i * 16, 16, 7, false);
                }
            }
        }
    }
    hud->score_previous = score;
    hud->combo_previous = combo;
    if (*c->high_score < score) {
        M_Number(hud, c, 0, 7, score, hud->high_previous, false);
        hud->high_previous = score;
        *c->high_score = score;
    }
    if (hud->maximum_combo < combo) {
        hud->maximum_combo = (uint8_t)combo;
        M_Number(
            hud, c, 2, 2, (uint32_t)(int32_t)combo,
            (uint32_t)(int32_t)hud->max_previous, true);
        hud->max_previous = combo;
    }
}

void Hud_SnapStage(HUD *const hud, const HUD_CONTEXT *const c)
{
    ASSERT(c->stage_number <= 99);
    M_Snap(hud->stage, c->video, 608, 32);
    char text[3];
    M_Format(text, c->stage_number, 2);
    M_Text(c, text, 608, 32, 7, true);
    M_Copy(c->video, 608, 32, 16);
}
void Hud_PutStage(HUD *const hud, const HUD_CONTEXT *const c)
{
    ASSERT(c->rank >= 0 && c->rank <= 3 && c->stage_number <= 99);
    char text[3];
    M_Format(text, c->stage_number, 2);
    const char *const rank = m_Ranks[c->rank];
    int32_t length = 0;
    while (rank[length] != '\0') {
        length++;
    }
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(c->video, page);
        M_Text(c, rank, 600 - length * 4, 48, 7, true);
        M_Put(hud->stage, c->video, 608, 32);
        M_Text(c, text, 608, 32, 7, true);
    }
}
void Hud_SnapNumbers(
    HUD *const hud, const HUD_CONTEXT *const c, const bool first_stage)
{
    for (int32_t bank = 1; bank >= 0; bank--) {
        if (first_stage) {
            Video_SelectDrawingPage(c->video, 1);
            for (int32_t i = 0; i < 7; i++) {
                M_Snap(
                    hud->numbers + (bank * 7 + i) * 256, c->video, 256 + i * 16,
                    bank * 16);
            }
            Video_SelectDrawingPage(c->video, 0);
            const uint32_t value =
                bank == 1 ? c->gameplay->score : *c->high_score;
            uint32_t divisor = 1000000;
            for (int32_t i = 0; i < 7; i++, divisor /= 10) {
                M_Text(
                    c, m_Numerals[value / divisor % 10], 256 + i * 16,
                    bank * 16, 7, bank == 0);
            }
            Video_SelectDrawingPage(c->video, 1);
        }
        for (int32_t i = 0; i < 2; i++) {
            uint8_t *const bg = hud->numbers + ((bank + 2) * 7 + i) * 256;
            if (first_stage) {
                M_Snap(bg, c->video, 400 + i * 16, bank * 16);
            } else {
                M_Put(bg, c->video, 400 + i * 16, bank * 16);
            }
        }
        if (first_stage || bank == 1) {
            Video_SelectDrawingPage(c->video, 0);
        }
        for (int32_t i = 0; i < 2; i++) {
            M_Text(c, m_Numerals[0], 400 + i * 16, bank * 16, 7, bank == 0);
        }
    }
    M_Copy(c->video, 256, 0, 176);
    M_Copy(c->video, 256, 16, 176);
    hud->maximum_combo = 0;
}
void Hud_SnapTimer(
    HUD *const hud, const HUD_CONTEXT *const c, const uint16_t time)
{
    M_Snap(hud->timer, c->video, 512, 0);
    M_Snap(hud->timer + 256, c->video, 512, 0);
    char text[5];
    M_Format(text, time, 4);
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(c->video, page);
        M_Text(c, text, 512, 0, 7, true);
    }
}
void Hud_PutTimer(
    HUD *const hud, const HUD_CONTEXT *const c, const uint16_t time)
{
    char text[5];
    M_Format(text, time, 4);
    for (int32_t page = 1; page >= 0; page--) {
        Video_SelectDrawingPage(c->video, page);
        M_Put(hud->timer, c->video, 512, 0);
        M_Put(hud->timer + 256, c->video, 528, 0);
        M_Text(c, text, 512, 0, time < 200 ? 10 : 7, true);
    }
}
