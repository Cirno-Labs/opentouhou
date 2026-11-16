#include "th01/wave.h"

#include "core/base/assert.h"
#include "th01/math.h"
#include "th01/sprite.h"

void Wave_Restore(
    VIDEO *const video, const int16_t left, const int16_t top,
    const int16_t wavelength, const int16_t amplitude, const int16_t phase,
    const int16_t width, const int16_t height)
{
    ASSERT(wavelength != 0 && width > 0);
    int16_t angle = phase;
    for (int32_t y = 0; y < height; y++) {
        const int16_t x = Math_Wrap16(left + Math_WaveX(amplitude, angle));
        angle = Math_Wrap16(angle + 256 / wavelength);
        Sprite_Restore16(video, x, Math_Wrap16(top + y), width, 1);
    }
}
