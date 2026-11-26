#include "th02/hud/numbers.h"

#include "th02/gaiji.h"
#include "th02/tram.h"

#include "core/base/assert.h"

void Hud_Number(
    const uint16_t left, const uint16_t top, const uint8_t digits,
    const int32_t value, const uint8_t attribute, const bool show_zero)
{
    ASSERT(digits <= 8);
    if (digits == 0) {
        return;
    }
    int32_t divisor = 10000000;
    for (uint8_t remaining = 8; remaining > digits; remaining--) {
        divisor /= 10;
    }
    bool significant = false;
    for (uint8_t i = 0; i < digits; i++) {
        const int32_t digit = (value / divisor) % 10;
        divisor /= 10;
        significant |= digit != 0;
        if (significant || (show_zero && i == digits - 1)) {
            Tram_PutGaiji(
                (uint16_t)(left + i * 2), top, (uint8_t)(GAIJI_DIGIT_0 + digit),
                attribute);
        }
    }
}
