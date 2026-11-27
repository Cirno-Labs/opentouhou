#include "th01/wave.h"

#include "th01/sprite.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

void Wave_Restore(
    const int16_t left, const int16_t top, const int16_t wavelength,
    const int16_t amplitude, const int16_t phase, const int16_t width,
    const int16_t height)
{
    ASSERT(wavelength != 0 && width > 0);
    int16_t angle = phase;
    for (int32_t y = 0; y < height; y++) {
        const int16_t x = Math16_Wrap(left + Math16_Wave(amplitude, angle));
        angle = Math16_Wrap(angle + 256 / wavelength);
        Sprite_Restore16(x, Math16_Wrap(top + y), width, 1);
    }
}
