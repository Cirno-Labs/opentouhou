#include "th02/player/render.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

void PlayerRender_Put(
    const PATTERN_BANK *const patterns, const ENTITY_POINT position,
    const int16_t scroll, const uint16_t sprite, const bool tiny)
{
    int16_t top = Math16_Wrap((int32_t)position.y + scroll);
    if (top >= 400) {
        top -= 400;
    }
    ASSERT(sprite < PATTERN_LIMIT);
    const PATTERN *const pattern = &patterns->entries[sprite];
    ASSERT(pattern->data != nullptr);
    if (tiny) {
        Draw_SpriteRollTiny(pattern, position.x, top);
    } else {
        Draw_SpriteRoll(pattern, position.x, top);
    }
}
