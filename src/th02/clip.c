#include "th02/clip_private.h"
#include "th02/video.h"

#include <string.h>

static CLIP m_Clip;
const CLIP *const g_Clip = &m_Clip;
CLIP *const p_Clip = &m_Clip;

static RESULT M_ClipAxis(
    const int16_t a, const int16_t b, const int16_t maximum, int16_t *const low,
    int16_t *const high)
{
    const int16_t first = a < b ? a : b;
    const int16_t last = a < b ? b : a;
    FAIL_IF(first > maximum || last < 0, "Clip axis is outside the screen");
    *low = first < 0 ? 0 : first;
    *high = last > maximum ? maximum : last;
    return OK;
}

void Clip_Init(CLIP *const clip)
{
    *clip = (CLIP) { .right = VIDEO_WIDTH - 1, .bottom = VIDEO_HEIGHT - 1 };
}

void Clip_Reset(void)
{
    Clip_Init(&m_Clip);
}

void Clip_SetPlayfield(void)
{
    memset(&m_Clip, 0, sizeof(m_Clip));
    m_Clip.left = 32;
    m_Clip.right = 416;
    m_Clip.bottom = 399;
}

void Clip_SetSidePanel(void)
{
    Result_Ignore(Clip_Set(&m_Clip, 416, 0, 639, 399));
}

void Clip_SetSummonArea(void)
{
    Result_Ignore(Clip_Set(&m_Clip, 32, 16, 416, 320));
}

void Clip_SetScoreTable(void)
{
    Result_Ignore(Clip_Set(&m_Clip, 128, 96, 512, 304));
}

RESULT Clip_Set(
    CLIP *const clip, const int16_t left, const int16_t top,
    const int16_t right, const int16_t bottom)
{
    MUST(M_ClipAxis(left, right, VIDEO_WIDTH - 1, &clip->left, &clip->right));
    MUST(M_ClipAxis(top, bottom, VIDEO_HEIGHT - 1, &clip->top, &clip->bottom));
    return OK;
}
