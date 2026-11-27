#pragma once

// Sets g_Clip directly for test setup, bypassing Clip_Set's screen
// clamp.
#include "th02/clip_private.h"

#include <string.h>

static inline void TestClip_Set(
    const int16_t left, const int16_t top, const int16_t right,
    const int16_t bottom)
{
    memset(p_Clip, 0, sizeof(*p_Clip));
    p_Clip->left = left;
    p_Clip->top = top;
    p_Clip->right = right;
    p_Clip->bottom = bottom;
}
