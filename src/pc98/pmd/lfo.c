#include "pc98/pmd/lfo.h"

#include <string.h>

static int M_Signed(const uint8_t value)
{
    return value < 128 ? value : (int)value - 256;
}

static void M_Depth(PMD_LFO *const lfo)
{
    lfo->depth_counter--;
    if (lfo->depth_counter != 0) {
        return;
    }
    lfo->depth_counter = lfo->depth_interval;
    if (lfo->depth_count == 0) {
        return;
    }
    if (lfo->depth_count < 128) {
        lfo->depth_count--;
    }
    if (lfo->delta >= 128) {
        const uint8_t magnitude = (uint8_t)(0 - lfo->delta);
        const uint8_t result = (uint8_t)(magnitude + lfo->depth);
        lfo->delta = result < 128 ? (uint8_t)(0 - result)
            : lfo->depth >= 128   ? 0
                                  : 0x81;
    } else {
        const uint8_t result = (uint8_t)(lfo->delta + lfo->depth);
        lfo->delta = result < 128 ? result : lfo->depth >= 128 ? 0 : 127;
    }
}

static void M_Update(PMD_LFO *const lfo, uint16_t *const random_seed)
{
    if (lfo->speed != 1) {
        if (lfo->speed != 255) {
            lfo->speed--;
        }
        return;
    }
    lfo->speed = lfo->initial_speed;
    if (lfo->wave == 0 || lfo->wave == 4 || lfo->wave == 5) {
        int delta = M_Signed(lfo->delta);
        if (lfo->wave == 5) {
            const uint8_t magnitude =
                lfo->delta < 128 ? lfo->delta : (uint8_t)(0 - lfo->delta);
            delta *= M_Signed(magnitude);
        }
        lfo->value = (uint16_t)(lfo->value + delta);
        if (lfo->value == 0) {
            M_Depth(lfo);
        }
        if (lfo->period != 255) {
            lfo->period--;
            if (lfo->period == 0) {
                lfo->period = lfo->wave == 4
                    ? lfo->initial_period
                    : (uint8_t)(lfo->initial_period * 2);
                lfo->delta = (uint8_t)(0 - lfo->delta);
            }
        }
    } else if (lfo->wave == 1) {
        lfo->value = (uint16_t)(lfo->value + M_Signed(lfo->delta));
        if (lfo->period != 255) {
            lfo->period--;
            if (lfo->period == 0) {
                lfo->value = (uint16_t)(0 - lfo->value);
                M_Depth(lfo);
                lfo->period = (uint8_t)(lfo->initial_period * 2);
            }
        }
    } else if (lfo->wave == 2) {
        lfo->value = (uint16_t)(M_Signed(lfo->delta) * M_Signed(lfo->period));
        M_Depth(lfo);
        lfo->delta = (uint8_t)(0 - lfo->delta);
    } else if (lfo->wave == 6) {
        if (lfo->period != 0) {
            if (lfo->period != 255) {
                lfo->period--;
            }
            lfo->value = (uint16_t)(lfo->value + M_Signed(lfo->delta));
        }
    } else {
        const uint8_t magnitude =
            lfo->delta < 128 ? lfo->delta : (uint8_t)(0 - lfo->delta);
        const uint16_t amplitude = (uint16_t)(magnitude * lfo->period);
        lfo->value =
            (uint16_t)(PmdRandom_Next(random_seed, (uint16_t)(amplitude * 2))
                       - amplitude);
        M_Depth(lfo);
    }
}

void PmdLfo_Init(PMD_LFO *const lfo)
{
    memset(lfo, 0, sizeof(*lfo));
}

void PmdLfo_Reset(PMD_LFO *const lfo)
{
    lfo->value = 0;
    lfo->delay = lfo->initial_delay;
    lfo->speed = lfo->initial_speed;
    lfo->delta = lfo->initial_delta;
    lfo->period = lfo->initial_period;
    lfo->depth_count = lfo->initial_depth_count;
    if (lfo->wave == 2 || lfo->wave == 3) {
        lfo->speed = 1;
    } else {
        lfo->speed++;
    }
}

void PmdLfo_Configure(PMD_LFO *const lfo, const uint8_t parameters[4])
{
    lfo->initial_delay = parameters[0];
    lfo->initial_speed = parameters[1];
    lfo->initial_delta = parameters[2];
    lfo->initial_period = parameters[3];
    PmdLfo_Reset(lfo);
}

uint8_t PmdLfo_Switch(PMD_LFO *const lfo, const uint8_t flags, uint8_t command)
{
    if (command >= 8) {
        command = 1;
    }
    PmdLfo_Reset(lfo);
    return (uint8_t)((flags & 0xf8) | command);
}

bool PmdLfo_Step(
    PMD_LFO *const lfo, const bool timer_sync, const uint8_t timer_delta,
    uint16_t *const random_seed)
{
    if (lfo->delay != 0) {
        lfo->delay--;
        return false;
    }
    const uint16_t before = lfo->value;
    const unsigned count = timer_sync ? timer_delta : 1;
    for (unsigned i = 0; i < count; i++) {
        M_Update(lfo, random_seed);
    }
    return before != lfo->value;
}
