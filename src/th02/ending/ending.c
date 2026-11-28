#include "th02/ending.h"

#include "th02/ending/credits.h"
#include "th02/ending/images.h"
#include "th02/ending/private.h"
#include "th02/ending/verdict.h"

#include "core/base/assert.h"

#include <string.h>

static ENDING m_Ending;
ENDING *const p_Ending = &m_Ending;

static void M_Fade(const PALETTE_FADE_KIND kind, const int16_t speed)
{
    Wait_Fade(&m_Ending.wait, kind, speed);
}

static void M_Finish(void)
{
    if (m_Ending.session->rank == SESSION_EXTRA_RANK) {
        Palette_SetTone(0);
    }
    EndingImages_ApplyPalette(&m_Ending.assets->images, ENDING_IMAGE_ALL_CLEAR);
    EndingImages_Draw(&m_Ending.assets->images, ENDING_IMAGE_ALL_CLEAR);
    M_Fade(PALETTE_BLACK_IN, 2);
    m_Ending.phase = ENDING_FINISH_IN;
}

void Ending_Init(void)
{
    memset(&m_Ending, 0, sizeof(m_Ending));
    Story_Init();
    Credits_Init();
    Verdict_Init();
}

void Ending_Begin(
    const ENDING_ASSETS *const assets, const TH02_SESSION *const session,
    const STORY_AUDIO music, void *const music_context,
    const bool extra_unlocked)
{
    ASSERT(session->stage == 127);
    ASSERT(session->shot_type < 3);
    ASSERT(
        g_Clip->left == 0 && g_Clip->top == 0 && g_Clip->right == 639
        && g_Clip->bottom == 399);
    Ending_Init();
    m_Ending.assets = assets;
    m_Ending.session = session;
    m_Ending.music = music;
    m_Ending.music_context = music_context;
    m_Ending.extra_unlocked = extra_unlocked;
    m_Ending.route =
        session->continues_used == 0 ? ENDING_STORY_GOOD : ENDING_STORY_BAD;
    Palette_Import(assets->title_palette);
    Grcg_Off();
    Video_SetSplit(400, 0, 0);
    for (uint8_t page = 0; page < 2; page++) {
        Video_AccessPage(page);
        Video_Clear();
    }
    Video_AccessPage(0);
    Video_ShowPage(0);
    Tram_Fill(' ', 0xe1);
    Wait_Delay(&m_Ending.wait, 100);
}

bool Ending_Update(
    const TH02_INPUT *const input, const bool music_active,
    const uint16_t measure)
{
    for (;;) {
        switch (m_Ending.phase) {
        case ENDING_PREPARE:
            if (!Wait_Ready(&m_Ending.wait, nullptr, 0)) {
                return false;
            }
            if (m_Ending.session->rank == SESSION_EXTRA_RANK) {
                if (!m_Ending.extra_unlocked) {
                    m_Ending.phase = ENDING_DONE;
                    return true;
                }
                M_Finish();
                return false;
            }
            Story_Begin(
                m_Ending.assets, m_Ending.session, m_Ending.music,
                m_Ending.music_context);
            m_Ending.phase = ENDING_STORY_SCENE;
            break;

        case ENDING_STORY_SCENE:
            if (!Story_Update(input, music_active, measure)) {
                return false;
            }
            Credits_Begin(m_Ending.assets);
            m_Ending.phase = ENDING_CREDITS;
            break;

        case ENDING_CREDITS:
            if (!Credits_Update(input, music_active, measure)) {
                return false;
            }
            Verdict_Begin(
                m_Ending.assets, m_Ending.session, m_Ending.route, 0x2f);
            m_Ending.phase = ENDING_VERDICT;
            break;

        case ENDING_VERDICT:
            if (!Verdict_Update(input)) {
                return false;
            }
            if (m_Ending.extra_unlocked
                && m_Ending.session->continues_used == 0) {
                M_Finish();
                return false;
            }
            Video_Clear();
            m_Ending.phase = ENDING_DONE;
            return true;

        case ENDING_FINISH_IN:
            if (!Wait_Ready(&m_Ending.wait, nullptr, 0)) {
                return false;
            }
            Wait_Delay(&m_Ending.wait, 150);
            m_Ending.phase = ENDING_FINISH_HOLD;
            return false;

        case ENDING_FINISH_HOLD:
            if (!Wait_Ready(&m_Ending.wait, nullptr, 0)) {
                return false;
            }
            const ENDING_IMAGE image =
                m_Ending.session->rank == SESSION_EXTRA_RANK
                ? ENDING_IMAGE_EXTRA_CLEAR
                : ENDING_IMAGE_EXTRA_UNLOCK;
            EndingImages_ApplyPalette(&m_Ending.assets->images, image);
            EndingImages_Draw(&m_Ending.assets->images, image);
            Wait_Key(&m_Ending.wait, input);
            m_Ending.phase = ENDING_FINISH_KEY;
            return false;

        case ENDING_FINISH_KEY:
            if (Wait_Ready(&m_Ending.wait, input, 0)) {
                M_Fade(PALETTE_BLACK_OUT, 5);
                m_Ending.phase = ENDING_FINISH_OUT;
            }
            return false;

        case ENDING_FINISH_OUT:
            if (!Wait_Ready(&m_Ending.wait, nullptr, 0)) {
                return false;
            }
            if (m_Ending.session->rank != SESSION_EXTRA_RANK) {
                Video_Clear();
            }
            m_Ending.phase = ENDING_DONE;
            return true;

        case ENDING_DONE:
            return true;
        }
    }
}
