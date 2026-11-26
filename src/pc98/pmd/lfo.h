#pragma once

#include "pc98/pmd/random.h"

typedef struct {
    uint16_t value;
    uint8_t delay, speed, delta, period;
    uint8_t initial_delay, initial_speed, initial_delta, initial_period;
    uint8_t wave, depth, depth_counter, depth_interval;
    uint8_t depth_count, initial_depth_count;
} PMD_LFO;

// Evolves KAJA PMD's word-valued LFO with byte counters and signed-byte
// delta/depth arithmetic. Wave 0 is triangle, 1 saw, 2 square, 3 random,
// 4 triangle without period doubling, 5 nonlinear triangle, and 6 one-shot.
// Other wave bytes follow the original random-wave fallback. Derives rules
// from published PMD source and the original driver:
// https://github.com/takamichih/pmdsrc
// Keeps flag selection, note/tie reset decisions and chip output separate.
void PmdLfo_Init(PMD_LFO *lfo);

// Clears value and reloads delay/speed/delta/period and depth_count from
// initial values. Primes speed to one for waves 2/3, otherwise increments
// it with byte wrap. Retains the depth-rate counter and other controls.
void PmdLfo_Reset(PMD_LFO *lfo);

// Sets the initial delay/speed/delta/period from F2's four bytes, then Reset.
// Retains wave and depth controls. Does not advance the LFO or RNG.
void PmdLfo_Configure(PMD_LFO *lfo, const uint8_t parameters[4]);

// Replaces the primary LFO flag bits 2..0 from an F1 byte, then Reset.
// A command with any high bit set selects one. Retains flag bits 7..3;
// returns the updated flags. Does not change channel mode registers.
uint8_t PmdLfo_Switch(PMD_LFO *lfo, uint8_t flags, uint8_t command);

// Decrements an outstanding delay once and does no other work. Otherwise
// runs one update, or timer_delta updates when timer_sync is set. Reports
// whether the final value differs from its value before those updates.
// All counters and value wrap at their stored widths; speed FF suspends.
// Random waves advance the supplied shared driver seed, even at amplitude
// zero. Allocates nothing, performs no I/O, and reads no clock or device.
// Timer delta is an explicit wrapping Timer A counter difference, not a
// measured or assumed frame cadence.
bool PmdLfo_Step(
    PMD_LFO *lfo, bool timer_sync, uint8_t timer_delta, uint16_t *random_seed);
