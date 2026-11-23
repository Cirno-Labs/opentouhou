#include "th01/ending_internal.h"
#include "th01/ending_text.h"
#include "core/base/assert.h"
#include <stdio.h>
#include <string.h>

static void M_Add(
    ENDING *const e, const ENDING_OPERATION_KIND kind, const int32_t value)
{
    ASSERT(e->count < ENDING_OPERATION_CAPACITY);
    e->operations[e->count++] =
        (ENDING_OPERATION) { .kind = kind, .value = value };
}

static void M_Wait(ENDING *const e, const int32_t frames)
{
    M_Add(e, END_WAIT, frames);
}

static void M_Picture(ENDING *const e, const int32_t q, const int32_t delay)
{
    M_Add(e, END_PICTURE, q);
    M_Wait(e, delay);
}

static void M_Fade(
    ENDING *const e, const int32_t start, const int32_t step,
    const int32_t delay)
{
    for (int32_t i = 1; i <= 20; i++) {
        M_Add(e, END_TONE, start + i * step);
        M_Wait(e, delay);
    }
}

static void M_Text(
    ENDING *const e, const char *const text, const int32_t x, const int32_t y,
    const int32_t style, const int32_t bytes)
{
    M_Add(e, END_TEXT, bytes);
    ENDING_OPERATION *const op = &e->operations[e->count - 1];
    op->text = text;
    op->left = x;
    op->top = y;
    op->style = style;
}

static void M_Type(
    ENDING *const e, const char *const text, const int32_t left,
    const int32_t top, const bool extra)
{
    int32_t x = left;
    for (const char *p = text;;) {
        if (*p == '\0') {
            if (extra) {
                M_Text(e, "", x, top, 15, 0);
                M_Wait(e, 3);
            }
            return;
        }
        const unsigned char first = (unsigned char)*p;
        const int32_t bytes = first < 0x80 ? 1
            : first < 0xe0                 ? 2
            : first < 0xf0                 ? 3
                                           : 4;
        M_Text(e, p, x, top, 15, bytes);
        M_Wait(e, 3);
        x += first < 0x80 ? 8 : 16;
        p += bytes;
    }
}

static void M_Captions(
    ENDING *const e, const char *const a, const char *const b, const bool extra)
{
    M_Type(
        e, a, (320 - ((int32_t)strlen(a) + (int32_t)extra) * 4 + 7) & ~7, 316,
        extra);
    M_Type(
        e, b, (320 - ((int32_t)strlen(b) + (int32_t)extra) * 4 + 7) & ~7, 348,
        extra);
}

static void M_Shake(ENDING *const e, const int32_t shake, const int32_t boom)
{
    for (int32_t i = 0; i <= shake; i++) {
        M_Add(e, END_SCROLL, (i & 3) == 0 ? 8 : 0);
        M_Picture(e, 1, 1);
        M_Add(e, END_TONE, (i & 1) != 0 ? 170 : 100);
    }
    for (int32_t i = 0; i <= boom; i++) {
        M_Add(e, END_SCROLL, (i & 3) == 0 ? 8 : 0);
        M_Picture(e, 1, 2);
        M_Add(e, END_PICTURE, (i & 1) != 0 ? 2 : 1);
    }
}

static void M_Done(ENDING *const e)
{
    M_Add(e, END_AUDIO, ENDING_AUDIO_FADE);
    M_Fade(e, 100, -5, 10);
    M_Add(e, END_CLEAR, 0);
    M_Add(e, END_AUDIO, ENDING_AUDIO_STOP);
    M_Add(e, END_AUDIO, ENDING_AUDIO_STAGE1);
}

static void M_Story(ENDING *const e)
{
    const bool makai = e->result.route == SESSION_END_MAKAI;
    M_Add(e, END_AUDIO, ENDING_AUDIO_IRIS);
    M_Add(e, END_TONE, 0);
    M_Add(e, END_LOAD, 0);
    M_Add(e, END_PICTURE, 0);
    M_Fade(e, 0, 5, 6);
    M_Wait(e, 100);
    M_Fade(e, 100, 5, 5);
    M_Wait(e, 100);
    M_Add(e, END_PICTURE, 1);
    M_Add(e, END_TONE, 100);
    M_Wait(e, 13);
    M_Picture(e, 2, 13);
    M_Picture(e, 3, 50);
    M_Add(e, END_LOAD, 1);
    M_Picture(e, 0, 10);
    M_Picture(e, 1, 10);
    M_Picture(e, 2, 100);
    M_Fade(e, 100, -5, 6);
    M_Add(e, END_CLEAR, 0);
    M_Add(e, END_TONE, 100);
    M_Add(e, END_LOAD, 2);
    for (int32_t i = 0; i < 6; i++) {
        M_Picture(e, 0, 20);
        M_Picture(e, 1, 20);
    }
    M_Picture(e, 0, 40);
    M_Picture(e, 2, 40);
    M_Add(e, END_LOAD, 3);
    M_Picture(e, 3, 70);
    for (int32_t i = 0; i < 100; i++) {
        if (i == 0 || i == 28 || i == 58 || i == 98) {
            M_Add(e, END_PICTURE, 0);
        }
        if (i == 20 || i == 50 || i == 90) {
            M_Add(e, END_PICTURE, 1);
        }
        if (i == 24 || i == 54 || i == 94) {
            M_Add(e, END_PICTURE, 2);
        }
        M_Wait(e, 2);
    }
    M_Picture(e, 3, 60);
    M_Picture(e, 0, 20);
    M_Add(e, END_PICTURE, 3);
    M_Add(e, END_TONE, 105);
    M_Add(e, END_LOAD, 4);
    M_Add(e, END_PICTURE, 0);
    for (int32_t i = 0; i < 4; i++) {
        M_Add(e, END_TONE, 110 + i * 5);
        M_Wait(e, 16);
    }
    for (int32_t i = 0; i < 15; i++) {
        M_Picture(e, 1, 8);
        M_Picture(e, 2, 8);
        M_Add(e, END_TONE, 130 + i * 5);
    }
    if (e->result.total != 0) {
        M_Add(e, END_LOAD, 5);
        M_Add(e, END_TONE, 200);
        M_Add(e, END_PICTURE, 0);
        M_Fade(e, 200, -5, 5);
        M_Wait(e, 120);
        if (!makai) {
            M_Shake(e, 64, 16);
            M_Shake(e, 32, 32);
            M_Add(e, END_SCROLL, 0);
            M_Wait(e, 50);
        } else {
            M_Add(e, END_LOAD, 8);
            for (int32_t i = 0; i < 40; i++) {
                M_Picture(e, 0, 3);
                M_Picture(e, 1, 3);
                if (i >= 20) {
                    M_Add(e, END_TONE, 100 + (i - 20) * 5);
                }
            }
            M_Add(e, END_PICTURE, 2);
            M_Fade(e, 200, -5, 5);
            M_Wait(e, 200);
        }
        M_Picture(e, 3, 100);
        M_Captions(
            e, m_BAD_ENDING_CAPTION,
            makai ? m_BAD_ENDING_NAME_MAKAI : m_BAD_ENDING_NAME_JIGOKU, false);
        M_Wait(e, 300);
        M_Done(e);
        return;
    }
    M_Add(e, END_LOAD, 6);
    M_Add(e, END_TONE, 200);
    for (int32_t i = 0; i < 20; i++) {
        M_Picture(e, 0, 8);
        M_Picture(e, 1, 8);
        M_Add(e, END_TONE, 200 - i * 5);
    }
    M_Add(e, END_TONE, 100);
    for (int32_t i = 0; i < 10; i++) {
        M_Picture(e, 0, 8);
        M_Picture(e, 1, 8);
    }
    for (int32_t i = 0; i < 10; i++) {
        M_Picture(e, i & 1, 8);
        M_Add(e, END_TONE, 100 + i * 10);
    }
    M_Add(e, END_TONE, 200);
    M_Add(e, END_LOAD, makai ? 9 : 7);
    M_Add(e, END_PICTURE, 0);
    M_Fade(e, 200, -5, 4);
    M_Wait(e, makai ? 300 : 250);
    if (makai) {
        for (int32_t i = 0; i < 15; i++) {
            M_Picture(e, 1, 8);
            M_Picture(e, 2, 8);
        }
        M_Add(e, END_LOAD, 10);
        M_Picture(e, 0, 150);
        M_Picture(e, 1, 100);
        M_Picture(e, 2, 100);
        M_Picture(e, 3, 100);
        M_Add(e, END_LOAD, 11);
        M_Picture(e, 0, 100);
        M_Picture(e, 1, 150);
        M_Picture(e, 2, 150);
    } else {
        M_Picture(e, 1, 200);
        M_Picture(e, 2, 150);
        M_Picture(e, 3, 150);
    }
    M_Captions(
        e, m_GOOD_ENDING_CAPTION,
        makai ? m_GOOD_ENDING_NAME_MAKAI : m_GOOD_ENDING_NAME_JIGOKU, false);
    M_Wait(e, 300);
    M_Done(e);
    M_Add(e, END_LOAD, makai ? 12 : 13);
    M_Add(e, END_PICTURE, 0);
    M_Add(e, END_TONE, 100);
    M_Wait(e, 50);
    M_Captions(e, m_BOSS_STAGE_5, m_BOSS_NAME_5, true);
    const char *const titles[] = { m_BOSS_STAGE_10, m_BOSS_STAGE_15,
                                   m_BOSS_STAGE_20 };
    const char *const bosses[] = {
        makai ? m_BOSS_NAME_10_MAKAI : m_BOSS_NAME_10_JIGOKU,
        makai ? m_BOSS_NAME_15_MAKAI : m_BOSS_NAME_15_JIGOKU,
        makai ? m_BOSS_NAME_20_MAKAI : m_BOSS_NAME_20_JIGOKU
    };
    for (int32_t i = 0; i < 3; i++) {
        M_Wait(e, 150);
        M_Fade(e, 100, -5, 5);
        M_Add(e, END_CLEAR, 0);
        M_Add(e, END_PICTURE, i + 1);
        M_Fade(e, 0, 5, 5);
        M_Wait(e, 50);
        M_Captions(e, titles[i], bosses[i], false);
    }
    M_Wait(e, 150);
}

static void M_Results(ENDING *const e)
{
    const bool makai = e->result.route == SESSION_END_MAKAI;
    static const char *const ranks[] = { " EASY ", "NORMAL", " HARD ",
                                         "LUNATIC" };
    const END_RESULT *const r = &e->result;
    snprintf(
        e->result_lines[0], 256, "%s%s", m_RESULT_DIFFICULTY, ranks[r->rank]);
    snprintf(
        e->result_lines[1], 256, "%s%7u", m_RESULT_HIGH_SCORE,
        r->highest_score);
    snprintf(e->result_lines[2], 256, "%s%7u", m_RESULT_FINAL_SCORE, r->score);
    snprintf(e->result_lines[3], 256, "%s", m_RESULT_CONTINUES);
    snprintf(
        e->result_lines[4], 256, "%s%3u", m_RESULT_SHRINE, r->continues[0]);
    snprintf(
        e->result_lines[5], 256, "%s%3u",
        makai ? m_RESULT_MAKAI_1 : m_RESULT_JIGOKU_1, r->continues[1]);
    snprintf(
        e->result_lines[6], 256, "%s%3u",
        makai ? m_RESULT_MAKAI_2 : m_RESULT_JIGOKU_2, r->continues[2]);
    snprintf(
        e->result_lines[7], 256, "%s%3u",
        makai ? m_RESULT_MAKAI_3 : m_RESULT_JIGOKU_3, r->continues[3]);
    snprintf(
        e->result_lines[8], 256, "%s%5u",
        makai ? m_RESULT_MAKAI_TOTAL : m_RESULT_JIGOKU_TOTAL, r->total);
    M_Fade(e, 100, -5, 10);
    M_Add(e, END_RESULT_LINES, 14);
    M_Fade(e, 0, 5, 8);
    M_Type(e, "東方靈異伝", 400, 32, false);
    M_Type(e, "ver", 496, 32, false);
    M_Type(e, "1.10", 528, 32, false);
    M_Wait(e, 30);
    for (int32_t i = 0; i < 9; i++) {
        M_Text(e, e->result_lines[i], 296, (i + 2) * 32, 15, 0);
        M_Wait(e, 30);
    }
    M_Type(e, m_RESULT_THANKS, 296, 352, false);
    M_Text(e, m_RESULT_TITLE_HEADING, 264, 384, 18, 0);
    M_Wait(e, 50);
    M_Add(e, END_TITLE, 0);
    M_Wait(e, 100);
    M_Add(e, END_INPUT, 0);
    M_Add(e, END_REGISTER, 0);
}

static void M_Load(ENDING *const e, const int32_t index)
{
    const PI_IMAGE *const sheet = &e->assets->sheets[index];
    memcpy(e->video->pages[1], sheet->pixels, VIDEO_PAGE_SIZE);
    Video_SelectDrawingPage(e->video, 1);
    Palette_Import(&e->palette.resource, sheet->palette);
    Palette_ShowResource(&e->palette, e->video, true);
}

void Ending_Prepare(
    const ALLOCATOR *const allocator, const ENDING_ASSETS *const assets,
    VIDEO *const video, FRAME_VSYNC *const vsync, GAME_INPUT *const input,
    const ENDING_AUDIO_CONSUMER audio, void *const audio_context,
    ENDING **const out)
{
    ASSERT(audio != nullptr);
    ENDING *const e = Allocator_AllocZ(allocator, sizeof(*e));
    e->assets = assets;
    e->video = video;
    e->vsync = vsync;
    e->input = input;
    e->audio = audio;
    e->audio_context = audio_context;
    *out = e;
}

void Ending_Begin(ENDING *const e, SESSION *const session)
{
    Ending_Snapshot(&e->result, session);
    Frame_SwitchProgram(e->vsync);
    e->next = 0;
    e->count = 0;
    e->waiting = false;
    e->sensing = false;
    e->timeout = 0;
    Palette_Init(&e->palette);
    Video_SelectVisiblePage(e->video, 0);
    Video_SetScroll(e->video, 0);
    M_Story(e);
    M_Results(e);
}

bool Ending_Tick(
    ENDING *const e, const TH01_INPUT *const raw, const bool reference_edge)
{
    if (reference_edge) {
        Frame_VSync(e->vsync);
    }
    while (e->next < e->count) {
        const ENDING_OPERATION *const op = &e->operations[e->next];
        switch (op->kind) {
        case END_LOAD:
            M_Load(e, op->value);
            break;
        case END_RESULT_LINES:
            M_Load(e, op->value);
            Video_Copy(e->video, 1, (GFX_RECT) { 0, 0, 640, 400 }, 0, 0, 0);
            Video_SelectDrawingPage(e->video, 0);
            break;
        case END_PICTURE:
            Video_Copy(
                e->video, 1,
                (GFX_RECT) { (op->value % 2) * 320, (op->value / 2) * 200, 320,
                             200 },
                0, 160, 100);
            Video_SelectDrawingPage(e->video, 0);
            break;
        case END_TONE:
            Palette_SetTone(&e->palette, e->video, op->value);
            break;
        case END_WAIT:
            if (!e->waiting) {
                e->vsync->count1 = 0;
                e->waiting = true;
            }
            if ((uint16_t)e->vsync->count1 < op->value) {
                return false;
            }
            e->waiting = false;
            break;
        case END_CLEAR:
            Video_SelectDrawingPage(e->video, 0);
            Video_Clear(e->video, 0);
            break;
        case END_SCROLL:
            Video_SetScroll(e->video, (int16_t)op->value);
            break;
        case END_AUDIO:
            e->audio(e->audio_context, (ENDING_AUDIO)op->value);
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
                &e->assets->font, e->video, text, op->left, op->top,
                (uint8_t)(op->style & 15), &style);
            break;
        }
        case END_TITLE: {
            Ending_CalculateTitle(&e->result);
            const GAME_TEXT_STYLE style = { .weight = GAME_TEXT_HEAVY };
            GameText_Draw(
                &e->assets->font, e->video,
                m_PlayerTitles[e->result.group][e->result.level], 488, 384, 2,
                &style);
            break;
        }
        case END_INPUT:
            if (!e->sensing) {
                GameInput_ResetGame(e->input);
                e->sensing = true;
            }
            if (e->waiting) {
                if ((uint16_t)e->vsync->count1 < 1) {
                    return false;
                }
                e->waiting = false;
            }
            if (e->timeout++ >= 2000 || e->input->ok || e->input->shot) {
                break;
            }
            GameInput_SenseEnding(e->input, raw);
            e->vsync->count1 = 0;
            e->waiting = true;
            return false;
        case END_REGISTER:
            Video_Copy(e->video, 1, (GFX_RECT) { 0, 0, 640, 400 }, 0, 0, 0);
            Video_SelectDrawingPage(e->video, 0);
            Palette_SetTone(&e->palette, e->video, 50);
            Palette_SetColor(&e->palette, e->video, 3, 7, 15, 7);
            Palette_SetColor(&e->palette, e->video, 7, 15, 15, 15);
            break;
        }
        e->next++;
    }
    return true;
}

const END_RESULT *Ending_Result(const ENDING *const e)
{
    return &e->result;
}

void Ending_BeginSwitch(ENDING *const e)
{
    Palette_BeginFade(&e->switch_fade, &e->palette, PALETTE_FADE_BLACK_OUT);
    Frame_BeginWait(&e->switch_wait, e->vsync);
}

bool Ending_SwitchTick(ENDING *const e, const bool reference_edge)
{
    if (reference_edge) {
        Frame_PollVSync(e->vsync);
    }
    if (!Frame_WaitReady(&e->switch_wait, e->vsync)) {
        return false;
    }
    Palette_FadeStep(&e->switch_fade, &e->palette, e->video);
    if (e->switch_fade.remaining > 0) {
        Frame_BeginWait(&e->switch_wait, e->vsync);
        return false;
    }
    return true;
}
