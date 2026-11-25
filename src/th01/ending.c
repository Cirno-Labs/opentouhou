#include "th01/ending_internal.h"
#include "th01/ending_text.h"
#include "th01/music.h"
#include "th01/sound.h"
#include "core/base/assert.h"
#include <stdio.h>
#include <string.h>

END_RESULT g_EndResult;

static ENDING m_Ending;
static const ENDING_ASSETS *m_Assets;

static void M_Audio(const ENDING_AUDIO command)
{
    switch (command) {
    case ENDING_AUDIO_IRIS:
        Sound_Load(17);
        Music_Play(17);
        break;
    case ENDING_AUDIO_FADE:
        Music_Fade();
        break;
    case ENDING_AUDIO_STOP:
        Music_Stop();
        break;
    case ENDING_AUDIO_STAGE1:
        Sound_Load(1);
        Music_Play(1);
        break;
    }
}

static void M_Add(const ENDING_OPERATION_KIND kind, const int32_t value)
{
    ASSERT(m_Ending.count < ENDING_OPERATION_CAPACITY);
    m_Ending.operations[m_Ending.count++] =
        (ENDING_OPERATION) { .kind = kind, .value = value };
}

static void M_Wait(const int32_t frames)
{
    M_Add(END_WAIT, frames);
}

static void M_Picture(const int32_t q, const int32_t delay)
{
    M_Add(END_PICTURE, q);
    M_Wait(delay);
}

static void M_Fade(const int32_t start, const int32_t step, const int32_t delay)
{
    for (int32_t i = 1; i <= 20; i++) {
        M_Add(END_TONE, start + i * step);
        M_Wait(delay);
    }
}

static void M_Text(
    const char *const text, const int32_t x, const int32_t y,
    const int32_t style, const int32_t bytes)
{
    M_Add(END_TEXT, bytes);
    ENDING_OPERATION *const op = &m_Ending.operations[m_Ending.count - 1];
    op->text = text;
    op->left = x;
    op->top = y;
    op->style = style;
}

static void M_Type(
    const char *const text, const int32_t left, const int32_t top,
    const bool extra)
{
    int32_t x = left;
    for (const char *p = text;;) {
        if (*p == '\0') {
            if (extra) {
                M_Text("", x, top, 15, 0);
                M_Wait(3);
            }
            return;
        }
        const unsigned char first = (unsigned char)*p;
        const int32_t bytes = first < 0x80 ? 1
            : first < 0xe0                 ? 2
            : first < 0xf0                 ? 3
                                           : 4;
        M_Text(p, x, top, 15, bytes);
        M_Wait(3);
        x += first < 0x80 ? 8 : 16;
        p += bytes;
    }
}

static void M_Captions(
    const char *const a, const char *const b, const bool extra)
{
    M_Type(
        a, (320 - ((int32_t)strlen(a) + (int32_t)extra) * 4 + 7) & ~7, 316,
        extra);
    M_Type(
        b, (320 - ((int32_t)strlen(b) + (int32_t)extra) * 4 + 7) & ~7, 348,
        extra);
}

static void M_Shake(const int32_t shake, const int32_t boom)
{
    for (int32_t i = 0; i <= shake; i++) {
        M_Add(END_SCROLL, (i & 3) == 0 ? 8 : 0);
        M_Picture(1, 1);
        M_Add(END_TONE, (i & 1) != 0 ? 170 : 100);
    }
    for (int32_t i = 0; i <= boom; i++) {
        M_Add(END_SCROLL, (i & 3) == 0 ? 8 : 0);
        M_Picture(1, 2);
        M_Add(END_PICTURE, (i & 1) != 0 ? 2 : 1);
    }
}

static void M_Done(void)
{
    M_Add(END_AUDIO, ENDING_AUDIO_FADE);
    M_Fade(100, -5, 10);
    M_Add(END_CLEAR, 0);
    M_Add(END_AUDIO, ENDING_AUDIO_STOP);
    M_Add(END_AUDIO, ENDING_AUDIO_STAGE1);
}

static void M_Story(void)
{
    const bool makai = g_EndResult.route == SESSION_END_MAKAI;
    M_Add(END_AUDIO, ENDING_AUDIO_IRIS);
    M_Add(END_TONE, 0);
    M_Add(END_LOAD, 0);
    M_Add(END_PICTURE, 0);
    M_Fade(0, 5, 6);
    M_Wait(100);
    M_Fade(100, 5, 5);
    M_Wait(100);
    M_Add(END_PICTURE, 1);
    M_Add(END_TONE, 100);
    M_Wait(13);
    M_Picture(2, 13);
    M_Picture(3, 50);
    M_Add(END_LOAD, 1);
    M_Picture(0, 10);
    M_Picture(1, 10);
    M_Picture(2, 100);
    M_Fade(100, -5, 6);
    M_Add(END_CLEAR, 0);
    M_Add(END_TONE, 100);
    M_Add(END_LOAD, 2);
    for (int32_t i = 0; i < 6; i++) {
        M_Picture(0, 20);
        M_Picture(1, 20);
    }
    M_Picture(0, 40);
    M_Picture(2, 40);
    M_Add(END_LOAD, 3);
    M_Picture(3, 70);
    for (int32_t i = 0; i < 100; i++) {
        if (i == 0 || i == 28 || i == 58 || i == 98) {
            M_Add(END_PICTURE, 0);
        }
        if (i == 20 || i == 50 || i == 90) {
            M_Add(END_PICTURE, 1);
        }
        if (i == 24 || i == 54 || i == 94) {
            M_Add(END_PICTURE, 2);
        }
        M_Wait(2);
    }
    M_Picture(3, 60);
    M_Picture(0, 20);
    M_Add(END_PICTURE, 3);
    M_Add(END_TONE, 105);
    M_Add(END_LOAD, 4);
    M_Add(END_PICTURE, 0);
    for (int32_t i = 0; i < 4; i++) {
        M_Add(END_TONE, 110 + i * 5);
        M_Wait(16);
    }
    for (int32_t i = 0; i < 15; i++) {
        M_Picture(1, 8);
        M_Picture(2, 8);
        M_Add(END_TONE, 130 + i * 5);
    }
    if (g_EndResult.total != 0) {
        M_Add(END_LOAD, 5);
        M_Add(END_TONE, 200);
        M_Add(END_PICTURE, 0);
        M_Fade(200, -5, 5);
        M_Wait(120);
        if (!makai) {
            M_Shake(64, 16);
            M_Shake(32, 32);
            M_Add(END_SCROLL, 0);
            M_Wait(50);
        } else {
            M_Add(END_LOAD, 8);
            for (int32_t i = 0; i < 40; i++) {
                M_Picture(0, 3);
                M_Picture(1, 3);
                if (i >= 20) {
                    M_Add(END_TONE, 100 + (i - 20) * 5);
                }
            }
            M_Add(END_PICTURE, 2);
            M_Fade(200, -5, 5);
            M_Wait(200);
        }
        M_Picture(3, 100);
        M_Captions(
            m_BAD_ENDING_CAPTION,
            makai ? m_BAD_ENDING_NAME_MAKAI : m_BAD_ENDING_NAME_JIGOKU, false);
        M_Wait(300);
        M_Done();
        return;
    }
    M_Add(END_LOAD, 6);
    M_Add(END_TONE, 200);
    for (int32_t i = 0; i < 20; i++) {
        M_Picture(0, 8);
        M_Picture(1, 8);
        M_Add(END_TONE, 200 - i * 5);
    }
    M_Add(END_TONE, 100);
    for (int32_t i = 0; i < 10; i++) {
        M_Picture(0, 8);
        M_Picture(1, 8);
    }
    for (int32_t i = 0; i < 10; i++) {
        M_Picture(i & 1, 8);
        M_Add(END_TONE, 100 + i * 10);
    }
    M_Add(END_TONE, 200);
    M_Add(END_LOAD, makai ? 9 : 7);
    M_Add(END_PICTURE, 0);
    M_Fade(200, -5, 4);
    M_Wait(makai ? 300 : 250);
    if (makai) {
        for (int32_t i = 0; i < 15; i++) {
            M_Picture(1, 8);
            M_Picture(2, 8);
        }
        M_Add(END_LOAD, 10);
        M_Picture(0, 150);
        M_Picture(1, 100);
        M_Picture(2, 100);
        M_Picture(3, 100);
        M_Add(END_LOAD, 11);
        M_Picture(0, 100);
        M_Picture(1, 150);
        M_Picture(2, 150);
    } else {
        M_Picture(1, 200);
        M_Picture(2, 150);
        M_Picture(3, 150);
    }
    M_Captions(
        m_GOOD_ENDING_CAPTION,
        makai ? m_GOOD_ENDING_NAME_MAKAI : m_GOOD_ENDING_NAME_JIGOKU, false);
    M_Wait(300);
    M_Done();
    M_Add(END_LOAD, makai ? 12 : 13);
    M_Add(END_PICTURE, 0);
    M_Add(END_TONE, 100);
    M_Wait(50);
    M_Captions(m_BOSS_STAGE_5, m_BOSS_NAME_5, true);
    const char *const titles[] = { m_BOSS_STAGE_10, m_BOSS_STAGE_15,
                                   m_BOSS_STAGE_20 };
    const char *const bosses[] = {
        makai ? m_BOSS_NAME_10_MAKAI : m_BOSS_NAME_10_JIGOKU,
        makai ? m_BOSS_NAME_15_MAKAI : m_BOSS_NAME_15_JIGOKU,
        makai ? m_BOSS_NAME_20_MAKAI : m_BOSS_NAME_20_JIGOKU
    };
    for (int32_t i = 0; i < 3; i++) {
        M_Wait(150);
        M_Fade(100, -5, 5);
        M_Add(END_CLEAR, 0);
        M_Add(END_PICTURE, i + 1);
        M_Fade(0, 5, 5);
        M_Wait(50);
        M_Captions(titles[i], bosses[i], false);
    }
    M_Wait(150);
}

static void M_Results(void)
{
    const bool makai = g_EndResult.route == SESSION_END_MAKAI;
    static const char *const ranks[] = { " EASY ", "NORMAL", " HARD ",
                                         "LUNATIC" };
    const END_RESULT *const r = &g_EndResult;
    snprintf(
        m_Ending.result_lines[0], 256, "%s%s", m_RESULT_DIFFICULTY,
        ranks[r->rank]);
    snprintf(
        m_Ending.result_lines[1], 256, "%s%7u", m_RESULT_HIGH_SCORE,
        r->highest_score);
    snprintf(
        m_Ending.result_lines[2], 256, "%s%7u", m_RESULT_FINAL_SCORE, r->score);
    snprintf(m_Ending.result_lines[3], 256, "%s", m_RESULT_CONTINUES);
    snprintf(
        m_Ending.result_lines[4], 256, "%s%3u", m_RESULT_SHRINE,
        r->continues[0]);
    snprintf(
        m_Ending.result_lines[5], 256, "%s%3u",
        makai ? m_RESULT_MAKAI_1 : m_RESULT_JIGOKU_1, r->continues[1]);
    snprintf(
        m_Ending.result_lines[6], 256, "%s%3u",
        makai ? m_RESULT_MAKAI_2 : m_RESULT_JIGOKU_2, r->continues[2]);
    snprintf(
        m_Ending.result_lines[7], 256, "%s%3u",
        makai ? m_RESULT_MAKAI_3 : m_RESULT_JIGOKU_3, r->continues[3]);
    snprintf(
        m_Ending.result_lines[8], 256, "%s%5u",
        makai ? m_RESULT_MAKAI_TOTAL : m_RESULT_JIGOKU_TOTAL, r->total);
    M_Fade(100, -5, 10);
    M_Add(END_RESULT_LINES, 14);
    M_Fade(0, 5, 8);
    M_Type("東方靈異伝", 400, 32, false);
    M_Type("ver", 496, 32, false);
    M_Type("1.10", 528, 32, false);
    M_Wait(30);
    for (int32_t i = 0; i < 9; i++) {
        M_Text(m_Ending.result_lines[i], 296, (i + 2) * 32, 15, 0);
        M_Wait(30);
    }
    M_Type(m_RESULT_THANKS, 296, 352, false);
    M_Text(m_RESULT_TITLE_HEADING, 264, 384, 18, 0);
    M_Wait(50);
    M_Add(END_TITLE, 0);
    M_Wait(100);
    M_Add(END_INPUT, 0);
    M_Add(END_REGISTER, 0);
}

static void M_Load(const int32_t index)
{
    const PI_IMAGE *const sheet = &m_Assets->sheets[index];
    memcpy(g_Video.pages[1], sheet->pixels, VIDEO_PAGE_SIZE);
    Video_SelectDrawingPage(1);
    Palette_Import(&m_Ending.palette.resource, sheet->palette);
    Palette_ShowResource(&m_Ending.palette, true);
}

void Ending_Init(const ENDING_ASSETS *const assets)
{
    memset(&m_Ending, 0, sizeof(m_Ending));
    memset(&g_EndResult, 0, sizeof(g_EndResult));
    m_Assets = assets;
}

void Ending_Begin(void)
{
    Ending_Snapshot(&g_EndResult);
    VSyncIrq_SwitchProgram();
    m_Ending.next = 0;
    m_Ending.count = 0;
    m_Ending.waiting = false;
    m_Ending.sensing = false;
    m_Ending.timeout = 0;
    Palette_Init(&m_Ending.palette);
    Video_SelectVisiblePage(0);
    Video_SetScroll(0);
    M_Story();
    M_Results();
}

bool Ending_Tick(const TH01_INPUT *const raw, const bool reference_edge)
{
    if (reference_edge) {
        VSyncIrq_Edge();
    }
    while (m_Ending.next < m_Ending.count) {
        const ENDING_OPERATION *const op = &m_Ending.operations[m_Ending.next];
        switch (op->kind) {
        case END_LOAD:
            M_Load(op->value);
            break;
        case END_RESULT_LINES:
            M_Load(op->value);
            Video_Copy(1, (GFX_RECT) { 0, 0, 640, 400 }, 0, 0, 0);
            Video_SelectDrawingPage(0);
            break;
        case END_PICTURE:
            Video_Copy(
                1,
                (GFX_RECT) { (op->value % 2) * 320, (op->value / 2) * 200, 320,
                             200 },
                0, 160, 100);
            Video_SelectDrawingPage(0);
            break;
        case END_TONE:
            Palette_SetTone(&m_Ending.palette, op->value);
            break;
        case END_WAIT:
            if (!m_Ending.waiting) {
                VSyncIrq_BeginDelay();
                m_Ending.waiting = true;
            }
            if ((uint16_t)g_VSync.count1 < op->value) {
                return false;
            }
            m_Ending.waiting = false;
            break;
        case END_CLEAR:
            Video_SelectDrawingPage(0);
            Video_Clear(0);
            break;
        case END_SCROLL:
            Video_SetScroll((int16_t)op->value);
            break;
        case END_AUDIO:
            M_Audio((ENDING_AUDIO)op->value);
            break;
        case END_TEXT: {
            char glyph[5];
            const char *text = op->text;
            if (op->value != 0) {
                memcpy(glyph, text, (size_t)op->value);
                glyph[op->value] = '\0';
                text = glyph;
            }
            const GAME_TEXT_STYLE style = { .weight = op->style == 18
                                                ? GAME_TEXT_HEAVY
                                                : GAME_TEXT_NORMAL };
            GameText_Draw(
                &m_Assets->font, text, op->left, op->top,
                (uint8_t)(op->style & 15), &style);
            break;
        }
        case END_TITLE: {
            Ending_CalculateTitle(&g_EndResult);
            const GAME_TEXT_STYLE style = { .weight = GAME_TEXT_HEAVY };
            GameText_Draw(
                &m_Assets->font,
                m_PlayerTitles[g_EndResult.group][g_EndResult.level], 488, 384,
                2, &style);
            break;
        }
        case END_INPUT:
            if (!m_Ending.sensing) {
                GameInput_ResetGame();
                m_Ending.sensing = true;
            }
            if (m_Ending.waiting) {
                if ((uint16_t)g_VSync.count1 < 1) {
                    return false;
                }
                m_Ending.waiting = false;
            }
            if (m_Ending.timeout++ >= 2000 || g_GameInput.ok
                || g_GameInput.shot) {
                break;
            }
            GameInput_SenseEnding(raw);
            VSyncIrq_BeginDelay();
            m_Ending.waiting = true;
            return false;
        case END_REGISTER:
            Video_Copy(1, (GFX_RECT) { 0, 0, 640, 400 }, 0, 0, 0);
            Video_SelectDrawingPage(0);
            Palette_SetTone(&m_Ending.palette, 50);
            Palette_SetColor(&m_Ending.palette, 3, 7, 15, 7);
            Palette_SetColor(&m_Ending.palette, 7, 15, 15, 15);
            break;
        }
        m_Ending.next++;
    }
    return true;
}

void Ending_BeginSwitch(void)
{
    Palette_BeginFade(
        &m_Ending.switch_fade, &m_Ending.palette, PALETTE_FADE_BLACK_OUT);
    VSyncIrq_BeginWait(&m_Ending.switch_wait);
}

bool Ending_SwitchTick(const bool reference_edge)
{
    if (reference_edge) {
        VSyncIrq_PollEdge();
    }
    if (!VSyncIrq_WaitReady(&m_Ending.switch_wait)) {
        return false;
    }
    Palette_FadeStep(&m_Ending.switch_fade, &m_Ending.palette);
    if (m_Ending.switch_fade.remaining > 0) {
        VSyncIrq_BeginWait(&m_Ending.switch_wait);
        return false;
    }
    return true;
}

void Ending_Close(void)
{
    memset(&m_Ending, 0, sizeof(m_Ending));
    memset(&g_EndResult, 0, sizeof(g_EndResult));
    m_Assets = nullptr;
}
