#include "th01/transition.h"

#include "core/base/assert.h"

#define M_FRAME_COUNT 130

static void M_DrawStep(
    PAGE_TRANSITION *const t, const TRANSITION_CONTEXT *const c)
{
    int16_t tip_left = t->first_left;
    int16_t tip_top = t->first_top;
    while (t->first_left >= 0 && t->first_top < VIDEO_HEIGHT) {
        Interleave_Put8(c->video, c->assets, t->first_left, t->first_top, 1);
        t->first_left -= 8;
        t->first_top += 8;
    }
    t->first_left = tip_left < VIDEO_WIDTH - 8 ? tip_left + 8 : tip_left;
    t->first_top = tip_left < VIDEO_WIDTH - 8 ? 0 : tip_top + 8;
    if (t->frame % 8 == 0) {
        for (int32_t color = 0; color < 16; color++) {
            for (int32_t component = 0; component < 3; component++) {
                int8_t *const value = &c->current->colors[color][component];
                const int8_t target = c->target->colors[color][component];
                if (*value != target) {
                    *value += *value > target ? -1 : 1;
                }
            }
        }
        Palette_Set(c->palette, c->video, c->current);
    }
    tip_left = t->second_left;
    tip_top = t->second_top;
    while (t->second_left >= 0 && t->second_top < VIDEO_HEIGHT) {
        Interleave_Put8(c->video, c->assets, t->second_left, t->second_top, 2);
        t->second_left -= 8;
        t->second_top += 8;
    }
    t->second_left = tip_top > 0 ? VIDEO_WIDTH - 8 : tip_left - 8;
    t->second_top = tip_top > 0 ? tip_top - 8 : tip_top;
    t->frame++;
}

bool Transition_BeginDiagonal(
    PAGE_TRANSITION *const t, const TRANSITION_CONTEXT *const c,
    const uint16_t step_ms, const uint64_t host_tick)
{
    ASSERT(!t->active);
    *t = (PAGE_TRANSITION) { .last_tick = host_tick,
                             .step_ms = step_ms,
                             .second_left = VIDEO_WIDTH - 8,
                             .second_top = VIDEO_HEIGHT - 8,
                             .active = true };
    M_DrawStep(t, c);
    if (step_ms == 0) {
        while (t->frame < M_FRAME_COUNT) {
            M_DrawStep(t, c);
        }
        t->active = false;
    }
    return !t->active;
}

bool Transition_Advance(
    PAGE_TRANSITION *const t, const TRANSITION_CONTEXT *const c,
    const uint64_t host_tick)
{
    if (!t->active) {
        return true;
    }
    ASSERT(host_tick >= t->last_tick);
    const uint64_t elapsed = host_tick - t->last_tick;
    t->last_tick = host_tick;
    const uint32_t delay = (uint32_t)t->step_ms * 3;
    const uint32_t remaining = (M_FRAME_COUNT + 1 - t->frame) * delay;
    uint64_t credit = elapsed >= (remaining - t->credit + 49u) / 50u
        ? remaining
        : t->credit + elapsed * 50;
    while (credit >= delay) {
        credit -= delay;
        if (t->frame == M_FRAME_COUNT) {
            t->active = false;
            t->credit = 0;
            return true;
        }
        M_DrawStep(t, c);
    }
    t->credit = (uint32_t)credit;
    return false;
}
