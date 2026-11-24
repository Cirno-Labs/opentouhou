#include "th01/transition.h"

#include "core/base/assert.h"

#include <string.h>

#define M_FRAME_COUNT 130

typedef struct {
    uint64_t last_tick;
    uint32_t credit;
    uint16_t step_ms, frame;
    int16_t first_left, first_top, second_left, second_top;
    bool active;
} PAGE_TRANSITION;

static PAGE_TRANSITION m_Transition;

static void M_DrawStep(
    PALETTE_STATE *const palette, PALETTE *const current,
    const PALETTE *const target, const INTERLEAVE_ASSETS *const assets)
{
    int16_t tip_left = m_Transition.first_left;
    int16_t tip_top = m_Transition.first_top;
    while (m_Transition.first_left >= 0
           && m_Transition.first_top < VIDEO_HEIGHT) {
        Interleave_Put8(
            assets, m_Transition.first_left, m_Transition.first_top, 1);
        m_Transition.first_left -= 8;
        m_Transition.first_top += 8;
    }
    m_Transition.first_left =
        tip_left < VIDEO_WIDTH - 8 ? tip_left + 8 : tip_left;
    m_Transition.first_top = tip_left < VIDEO_WIDTH - 8 ? 0 : tip_top + 8;
    if (m_Transition.frame % 8 == 0) {
        for (int32_t color = 0; color < 16; color++) {
            for (int32_t component = 0; component < 3; component++) {
                int8_t *const value = &current->colors[color][component];
                const int8_t target_value = target->colors[color][component];
                if (*value != target_value) {
                    *value += *value > target_value ? -1 : 1;
                }
            }
        }
        Palette_Set(palette, current);
    }
    tip_left = m_Transition.second_left;
    tip_top = m_Transition.second_top;
    while (m_Transition.second_left >= 0
           && m_Transition.second_top < VIDEO_HEIGHT) {
        Interleave_Put8(
            assets, m_Transition.second_left, m_Transition.second_top, 2);
        m_Transition.second_left -= 8;
        m_Transition.second_top += 8;
    }
    m_Transition.second_left = tip_top > 0 ? VIDEO_WIDTH - 8 : tip_left - 8;
    m_Transition.second_top = tip_top > 0 ? tip_top - 8 : tip_top;
    m_Transition.frame++;
}

bool Transition_BeginDiagonal(
    PALETTE_STATE *const palette, PALETTE *const current,
    const PALETTE *const target, const INTERLEAVE_ASSETS *const assets,
    const uint16_t step_ms, const uint64_t host_tick)
{
    ASSERT(!m_Transition.active);
    memset(&m_Transition, 0, sizeof(m_Transition));
    m_Transition.last_tick = host_tick;
    m_Transition.step_ms = step_ms;
    m_Transition.second_left = VIDEO_WIDTH - 8;
    m_Transition.second_top = VIDEO_HEIGHT - 8;
    m_Transition.active = true;
    M_DrawStep(palette, current, target, assets);
    if (step_ms == 0) {
        while (m_Transition.frame < M_FRAME_COUNT) {
            M_DrawStep(palette, current, target, assets);
        }
        m_Transition.active = false;
    }
    return !m_Transition.active;
}

bool Transition_Advance(
    PALETTE_STATE *const palette, PALETTE *const current,
    const PALETTE *const target, const INTERLEAVE_ASSETS *const assets,
    const uint64_t host_tick)
{
    if (!m_Transition.active) {
        return true;
    }
    ASSERT(host_tick >= m_Transition.last_tick);
    const uint64_t elapsed = host_tick - m_Transition.last_tick;
    m_Transition.last_tick = host_tick;
    const uint32_t delay = (uint32_t)m_Transition.step_ms * 3;
    const uint32_t remaining = (M_FRAME_COUNT + 1 - m_Transition.frame) * delay;
    uint64_t credit = elapsed >= (remaining - m_Transition.credit + 49u) / 50u
        ? remaining
        : m_Transition.credit + elapsed * 50;
    while (credit >= delay) {
        credit -= delay;
        if (m_Transition.frame == M_FRAME_COUNT) {
            m_Transition.active = false;
            m_Transition.credit = 0;
            return true;
        }
        M_DrawStep(palette, current, target, assets);
    }
    m_Transition.credit = (uint32_t)credit;
    return false;
}

void Transition_Init(void)
{
    memset(&m_Transition, 0, sizeof(m_Transition));
}
