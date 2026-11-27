#pragma once

// Sets g_Clip directly for test setup, bypassing Clip_Set's screen
// clamp.
#include "th02/clip.h"

#include <string.h>

static inline void TestClip_Set(
    const int16_t left, const int16_t top, const int16_t right,
    const int16_t bottom)
{
    CLIP *const clip = (CLIP *)g_Clip;
    memset(clip, 0, sizeof(*clip));
    clip->left = left;
    clip->top = top;
    clip->right = right;
    clip->bottom = bottom;
}
