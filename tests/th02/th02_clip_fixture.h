#pragma once

// g_Clip is read-only outside th02/clip.c; this fixture pulls in the
// module's private storage so tests can poke it directly for setup,
// bypassing Clip_Set's screen clamp.
#include "th02/clip.c"

static inline void TestClip_Set(
    const int16_t left, const int16_t top, const int16_t right,
    const int16_t bottom)
{
    memset(&m_Clip, 0, sizeof(m_Clip));
    m_Clip.left = left;
    m_Clip.top = top;
    m_Clip.right = right;
    m_Clip.bottom = bottom;
}
