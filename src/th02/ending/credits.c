#include "th02/ending/credits.h"

#include "th02/ending/images.h"
#include "th02/ending/private_credits.h"
#include "th02/ending/render.h"
#include "th02/ending/text.h"
#include "th02/wait.h"

#include <string.h>

static const struct {
    uint16_t measure;
    uint8_t speed;
    uint8_t angle;
    uint8_t quarter;
    int8_t color;
    int8_t section;
} m_Pictures[] = {
    { 13, 4, 41, 0, -1, -1 },
    { 17, 252, 41, 2, ENDING_COLOR_CREDITS_1_B, -1 },
    { 21, 4, 41, 3, ENDING_COLOR_CREDITS_1_C, -1 },
    { 25, 252, 41, 0, ENDING_COLOR_CREDITS_2_A, ENDING_CREDIT_GRAPHICS },
    { 29, 8, 233, 1, ENDING_COLOR_CREDITS_2_B, -1 },
    { 33, 248, 233, 2, -1, -1 },
    { 37, 8, 233, 3, ENDING_COLOR_CREDITS_2_B, ENDING_CREDIT_MUSIC },
    { 41, 248, 233, 0, -1, -1 },
    { 45, 12, 41, 1, ENDING_COLOR_CREDITS_3_A, -1 },
    { 49, 244, 41, 2, ENDING_COLOR_CREDITS_3_B, ENDING_CREDIT_TESTERS },
    { 53, 12, 41, 3, ENDING_COLOR_CREDITS_3_C, -1 },
};

static CREDITS m_Credits;
CREDITS *const p_Credits = &m_Credits;

static void M_Delay(const uint16_t frames)
{
    Wait_Delay(&m_Credits.wait, frames);
}

static void M_Measure(
    const uint16_t measure, const CREDITS_PHASE next, const bool active)
{
    m_Credits.after_measure = next;
    m_Credits.phase = CREDITS_MEASURE;
    Wait_Measure(&m_Credits.wait, measure, active);
}

static void M_Title(const int16_t top)
{
    for (uint16_t i = 0; i < 5; i++) {
        Draw_SpriteOpaque(
            &m_Credits.assets->title.entries[m_Credits.title_frame * 5 + i],
            (int16_t)(432 + i * 16), top);
    }
}

static void M_Version(const int16_t top)
{
    const DRAW_TEXT *const version =
        &m_Credits.assets->labels.text[ENDING_LABEL_VERSION];
    Draw_Text(version->glyphs, version->count, 528, top, 0x2f);
}

static void M_Stage(const ENDING_IMAGE image, const bool palette)
{
    EndingRender_Stage(&m_Credits.assets->images, image, palette);
    Video_AccessPage(0);
}

static void M_Picture(void)
{
    const uint8_t image = m_Credits.image;
    if (image == 0) {
        M_Stage(ENDING_IMAGE_CREDITS_1, true);
    }
    if (image == 2 || image == 5) {
        EndingRender_Copy(32, 100, m_Pictures[image].quarter, 0, 200);
    }
    if (image == 7) {
        EndingImages_ApplyPalette(
            &m_Credits.assets->images, ENDING_IMAGE_CREDITS_3);
    }
    if (m_Pictures[image].section >= 0) {
        EndingRender_Credits(
            &m_Credits.assets->labels,
            (ENDING_CREDIT)m_Pictures[image].section);
    }
    if (m_Pictures[image].color >= 0) {
        EndingImages_Recolor(
            &m_Credits.assets->images, (ENDING_COLOR)m_Pictures[image].color);
    }
    EndingRender_BeginRotation(
        &m_Credits.rotation, m_Pictures[image].speed, m_Pictures[image].angle);
}

void Credits_Init(void)
{
    memset(&m_Credits, 0, sizeof(m_Credits));
}

void Credits_Begin(const ENDING_ASSETS *const assets)
{
    Credits_Init();
    m_Credits.assets = assets;
    m_Credits.title_top = 184;
}

bool Credits_Update(
    const TH02_INPUT *const input, const bool music_active,
    const uint16_t measure)
{
    for (;;) {
        switch (m_Credits.phase) {
        case CREDITS_BEGIN:
            M_Measure(6, CREDITS_TITLE, music_active);
            return false;

        case CREDITS_MEASURE:
            if (!Wait_Ready(&m_Credits.wait, input, measure)) {
                return false;
            }
            m_Credits.phase = m_Credits.after_measure;
            break;

        case CREDITS_TITLE:
            if (!Wait_Ready(&m_Credits.wait, nullptr, 0)) {
                return false;
            }
            if (m_Credits.title_frame == 11) {
                m_Credits.title_frame = 10;
                M_Version(192);
                M_Measure(8, CREDITS_MOVE, music_active);
                return false;
            }
            M_Title(184);
            m_Credits.title_frame++;
            M_Delay(4);
            return false;

        case CREDITS_MOVE:
            if (!Wait_Ready(&m_Credits.wait, nullptr, 0)) {
                return false;
            }
            if (m_Credits.title_top == 368) {
                M_Measure(9, CREDITS_HEADING, music_active);
                return false;
            }
            Grcg_SetColor(0);
            Draw_BoxFill(
                g_Clip, 528, m_Credits.title_top + 4, 591,
                m_Credits.title_top + 19);
            Grcg_Off();
            M_Title(m_Credits.title_top);
            M_Version(m_Credits.title_top + 8);
            m_Credits.title_top += 4;
            M_Delay(1);
            return false;

        case CREDITS_HEADING:
            EndingText_Begin(
                &m_Credits.text,
                &m_Credits.assets->labels.text[ENDING_LABEL_CREDITS_TITLE], 432,
                192, 20, 12, 0x2f, false);
            m_Credits.phase = CREDITS_TEXT;
            break;

        case CREDITS_TEXT: {
            if (!Wait_Ready(&m_Credits.wait, nullptr, 0)) {
                return false;
            }
            ENDING_TEXT_EVENT event;
            do {
                event = EndingText_Step(&m_Credits.text, input);
            } while (event.wait == ENDING_TEXT_INPUT);
            if (event.wait == ENDING_TEXT_DELAY) {
                M_Delay(event.frames);
                return false;
            }
            M_Measure(13, CREDITS_PICTURE, music_active);
            return false;
        }

        case CREDITS_PICTURE:
            M_Picture();
            m_Credits.phase = CREDITS_ROTATION;
            break;

        case CREDITS_ROTATION:
            if (!Wait_Ready(&m_Credits.wait, nullptr, 0)) {
                return false;
            }
            if (!EndingRender_StepRotation(&m_Credits.rotation)) {
                M_Delay(1);
                return false;
            }
            EndingRender_Copy(
                32, 100, m_Pictures[m_Credits.image].quarter, 0, 200);
            M_Delay(4);
            m_Credits.phase = CREDITS_PICTURE_HOLD;
            return false;

        case CREDITS_PICTURE_HOLD:
            if (!Wait_Ready(&m_Credits.wait, nullptr, 0)) {
                return false;
            }
            Palette_SetTone(100);
            if (m_Credits.image == 0) {
                EndingRender_Credits(
                    &m_Credits.assets->labels, ENDING_CREDIT_PROGRAM);
            } else if (m_Credits.image == 2 || m_Credits.image == 6) {
                M_Stage(
                    m_Credits.image == 2 ? ENDING_IMAGE_CREDITS_2
                                         : ENDING_IMAGE_CREDITS_3,
                    false);
            }
            m_Credits.image++;
            M_Measure(
                m_Credits.image == ARRAY_SIZE(m_Pictures)
                    ? 57
                    : m_Pictures[m_Credits.image].measure,
                m_Credits.image == ARRAY_SIZE(m_Pictures) ? CREDITS_BLACK_OUT
                                                          : CREDITS_PICTURE,
                music_active);
            return false;

        case CREDITS_BLACK_OUT:
            Wait_Fade(&m_Credits.wait, PALETTE_BLACK_OUT, 4);
            m_Credits.phase = CREDITS_FADE;
            return false;

        case CREDITS_FADE:
            if (!Wait_Ready(&m_Credits.wait, nullptr, 0)) {
                return false;
            }
            m_Credits.phase = CREDITS_DONE;
            return true;

        case CREDITS_DONE:
            return true;
        }
    }
}
