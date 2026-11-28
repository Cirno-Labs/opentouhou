#include "th02/ending/story.h"

#include "th02/ending/images.h"
#include "th02/ending/private_story.h"
#include "th02/ending/render.h"
#include "th02/ending/text.h"
#include "th02/wait.h"

#include "core/base/assert.h"

#include <string.h>

static const uint8_t m_BadColors[51] = {
    [6] = 6,  [10] = 6, [14] = 6, [15] = 6, [18] = 6, [19] = 6, [20] = 6,
    [22] = 6, [24] = 6, [27] = 6, [28] = 6, [31] = 6, [34] = 6, [36] = 6,
    [39] = 6, [42] = 6, [44] = 6, [45] = 6, [48] = 6,
};
static const uint8_t m_GoodColors[94] = {
    [14] = 9, [15] = 9, [17] = 9, [20] = 9, [21] = 9, [23] = 9, [24] = 9,
    [26] = 9, [27] = 9, [30] = 9, [31] = 9, [37] = 9, [38] = 9, [48] = 9,
    [49] = 9, [51] = 9, [54] = 9, [56] = 9, [58] = 9, [59] = 9, [60] = 9,
    [61] = 9, [71] = 9, [73] = 9, [76] = 9, [78] = 9, [80] = 9, [82] = 9,
};

static const uint8_t m_FirstBranch[ENDING_STORY_COUNT][3] = {
    { 27, 34, 42 },
    { 27, 48, 71 },
};
static const uint8_t m_LastLine[ENDING_STORY_COUNT][3] = {
    { 33, 41, 50 },
    { 47, 70, 93 },
};

static STORY m_Story;
STORY *const p_Story = &m_Story;

static void M_Delay(const uint16_t frames, const STORY_PHASE next)
{
    Wait_Delay(&m_Story.wait, frames);
    m_Story.after_wait = next;
    m_Story.phase = STORY_DELAY;
}

static void M_Fade(
    const PALETTE_FADE_KIND kind, const int16_t speed, const STORY_PHASE next)
{
    Wait_Fade(&m_Story.wait, kind, speed);
    m_Story.after_wait = next;
    m_Story.phase = STORY_FADE;
}

static void M_Music(const STORY_MUSIC song)
{
    if (m_Story.music != nullptr) {
        m_Story.music(m_Story.music_context, song);
    }
}

static void M_Stage(const ENDING_IMAGE image)
{
    EndingRender_Stage(&m_Story.assets->images, image, true);
}

static void M_Picture(const uint8_t quarter)
{
    EndingRender_Copy(160, 100, quarter, 0, 200);
}

static void M_BeforeLine(void)
{
    const uint8_t line = m_Story.line;
    m_Story.phase = STORY_LINE_BEGIN;
    if (line == 1) {
        if (m_Story.route == ENDING_STORY_GOOD) {
            M_Stage(ENDING_IMAGE_STORY_3);
        }
        M_Picture(m_Story.route == ENDING_STORY_GOOD ? 0 : 1);
        M_Fade(PALETTE_BLACK_IN, 1, STORY_LINE_BEGIN);
    } else if (m_Story.route == ENDING_STORY_BAD) {
        if (line == 4) {
            M_Picture(2);
        } else if (line == 13) {
            m_Story.phase = STORY_SCROLL;
        } else if (line == 24) {
            M_Stage(ENDING_IMAGE_STORY_2);
            M_Fade(PALETTE_BLACK_OUT, 2, STORY_CHANGE_PICTURE);
        } else if (line == 31) {
            M_Picture(1);
        }
    } else {
        if (line == 4 || line == 20) {
            M_Picture(1);
        } else if (line == 10 || line == 28 || line == 52 || line == 74) {
            M_Picture(2);
        } else if (line == 12 || line == 29 || line == 53 || line == 75) {
            M_Picture(3);
        } else if (line == 14) {
            EndingImages_Recolor(&m_Story.assets->images, ENDING_COLOR_STORY_3);
        } else if (line == 16 || line == 40 || line == 67 || line == 84) {
            M_Fade(PALETTE_BLACK_OUT, 2, STORY_CHANGE_PICTURE);
        }
    }
}

static void M_Line(void)
{
    const bool good = m_Story.route == ENDING_STORY_GOOD;
    const uint8_t last = m_LastLine[m_Story.route][m_Story.shot];
    const uint8_t color =
        good ? m_GoodColors[m_Story.line] : m_BadColors[m_Story.line];
    const uint16_t speed = good
        ? (m_Story.line == last || m_Story.line == 69 ? 12 : 6)
        : (m_Story.line == last           ? 9
               : m_Story.line == last - 1 ? 12
                                          : 6);
    const ENDING_TEXT_SCRIPT *const script =
        &m_Story.assets->story[m_Story.route];
    ASSERT(m_Story.line < script->count);
    EndingText_Begin(
        &m_Story.text, &script->lines[m_Story.line], 144, 332, 44, speed,
        (uint16_t)(0x20 | (color == 0 ? 15 : color)), m_Story.line != last);
    m_Story.phase = STORY_LINE;
}

static void M_AfterLine(void)
{
    const uint8_t previous = m_Story.line;
    if (previous == m_LastLine[m_Story.route][m_Story.shot]) {
        M_Delay(30, STORY_WHITE_OUT);
        return;
    }
    m_Story.line = previous == 26 ? m_FirstBranch[m_Story.route][m_Story.shot]
                                  : (uint8_t)(previous + 1);
    m_Story.phase = STORY_BEFORE_LINE;
    if (previous == 0) {
        M_Delay(20, STORY_INTRO_BLACK_OUT);
    } else if (m_Story.route == ENDING_STORY_BAD && previous == 11) {
        M_Delay(20, STORY_BEFORE_LINE);
    } else if (m_Story.route == ENDING_STORY_GOOD) {
        if (previous == 26) {
            M_Delay(10, STORY_BEFORE_LINE);
        } else if (previous == 27 || previous == 51 || previous == 73) {
            M_Delay(30, STORY_BEFORE_LINE);
        }
    }
}

static void M_Scroll(void)
{
    Video_AccessPage(0);
    uint8_t *const page = g_Video.pages[0];
    for (size_t y = 300; y-- > 102;) {
        memmove(page + y * 640 + 160, page + (y - 2) * 640 + 160, 320);
    }
    EndingRender_Copy(160, 100, 3, (int16_t)(198 - m_Story.scroll * 2), 2);
    m_Story.scroll++;
    M_Delay(1, STORY_SCROLL);
}

static void M_Shift(void)
{
    uint8_t *const page = g_Video.pages[0];
    for (size_t y = 100; y < 300; y++) {
        memmove(
            page + y * 640 + m_Story.left - 4, page + y * 640 + m_Story.left,
            320);
    }
    Grcg_SetColor(0);
    Draw_BoxFill(g_Clip, m_Story.left + 312, 100, m_Story.left + 319, 299);
    Grcg_Off();
    m_Story.left -= 4;
    M_Delay(1, STORY_SHIFT);
}

void Story_Init(void)
{
    memset(&m_Story, 0, sizeof(m_Story));
}

void Story_Begin(
    const ENDING_ASSETS *const assets, const TH02_SESSION *const session,
    const STORY_AUDIO music, void *const music_context)
{
    ASSERT(session->shot_type <= 2);
    Story_Init();
    m_Story.assets = assets;
    m_Story.music = music;
    m_Story.music_context = music_context;
    m_Story.route =
        session->continues_used == 0 ? ENDING_STORY_GOOD : ENDING_STORY_BAD;
    m_Story.shot = session->shot_type;
    m_Story.left = 160;
}

bool Story_Update(
    const TH02_INPUT *const input, const bool music_active,
    const uint16_t measure)
{
    for (;;) {
        switch (m_Story.phase) {
        case STORY_BEGIN:
            M_Music(STORY_MUSIC_INTRO);
            Palette_SetTone(0);
            M_Stage(ENDING_IMAGE_STORY_1);
            M_Fade(PALETTE_BLACK_IN, 2, STORY_INTRO_HOLD);
            break;

        case STORY_INTRO_HOLD:
            M_Delay(40, STORY_INTRO_PICTURE);
            break;

        case STORY_INTRO_PICTURE:
            M_Picture(0);
            M_Line();
            break;

        case STORY_INTRO_BLACK_OUT:
            M_Fade(PALETTE_BLACK_OUT, 1, STORY_BEFORE_LINE);
            break;

        case STORY_BEFORE_LINE:
            M_BeforeLine();
            break;

        case STORY_LINE_BEGIN:
            M_Line();
            break;

        case STORY_LINE: {
            const ENDING_TEXT_EVENT event =
                EndingText_Step(&m_Story.text, input);
            if (event.wait == ENDING_TEXT_DELAY) {
                M_Delay(event.frames, STORY_LINE);
            } else if (event.wait == ENDING_TEXT_DONE) {
                m_Story.phase = STORY_AFTER_LINE;
            }
            break;
        }

        case STORY_AFTER_LINE:
            M_AfterLine();
            break;

        case STORY_CHANGE_PICTURE:
            if (m_Story.route == ENDING_STORY_BAD) {
                M_Picture(m_Story.shot == 0 ? 0 : m_Story.shot + 1);
            } else {
                M_Stage(
                    m_Story.line == 16 ? ENDING_IMAGE_STORY_4
                                       : ENDING_IMAGE_STORY_5);
                M_Picture(m_Story.line == 16 ? 0 : m_Story.shot);
            }
            M_Fade(PALETTE_BLACK_IN, 2, STORY_LINE_BEGIN);
            break;

        case STORY_SCROLL:
            if (m_Story.scroll == 100) {
                m_Story.phase = STORY_LINE_BEGIN;
            } else {
                M_Scroll();
            }
            break;

        case STORY_WHITE_OUT:
            M_Fade(PALETTE_WHITE_OUT, 1, STORY_CREDITS_MUSIC);
            break;

        case STORY_CREDITS_MUSIC:
            M_Music(STORY_MUSIC_CREDITS);
            EndingText_Clear();
            M_Fade(PALETTE_WHITE_IN, 4, STORY_MEASURE_BEGIN);
            break;

        case STORY_MEASURE_BEGIN:
            Wait_Measure(&m_Story.wait, 5, music_active);
            m_Story.phase = STORY_MEASURE;
            break;

        case STORY_MEASURE:
            if (!Wait_Ready(&m_Story.wait, input, measure)) {
                return false;
            }
            m_Story.phase = STORY_SHIFT;
            break;

        case STORY_SHIFT:
            if (m_Story.left <= 36) {
                m_Story.phase = STORY_DONE;
                return true;
            }
            M_Shift();
            break;

        case STORY_DELAY:
            if (!Wait_Ready(&m_Story.wait, nullptr, 0)) {
                return false;
            }
            m_Story.phase = m_Story.after_wait;
            break;

        case STORY_FADE:
            if (!Wait_Ready(&m_Story.wait, nullptr, 0)) {
                return false;
            }
            m_Story.phase = m_Story.after_wait;
            break;

        case STORY_DONE:
            return true;
        }
    }
}
