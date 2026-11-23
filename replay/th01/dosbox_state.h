#pragma once

#include "addresses.h"
#include "harness.h"

#include "pc98/vsync.h"
#include "th01/frame.h"

#include <stdint.h>

// Holds the dosbox port's context: the virtual clock's VSync phase, the
// frames it began, the frame of the last OP menu call, the music test's
// track starts since OP began, the segment of the
// resident block once found, the run's choices as OP's options held
// them last, whether REIIDEN has run, whether the running REIIDEN has
// set its cards up, and the last cards' count and hash.
typedef struct {
    VSYNC_CLOCK clock;
    uint32_t frames;
    uint32_t menu_frame;
    uint32_t track_starts;
    uint16_t resident;
    uint32_t choices[4];
    bool reiiden_ran;
    bool cards_set;
    uint32_t card_count;
    uint32_t card_hash;
} TH01_DOSBOX;

// Reads the resident block into out and returns whether it exists; then
// th01->resident gives its segment.
bool Th01Dosbox_Resident(TH01_DOSBOX *th01, RETAIL_RESIDENT *out);

// Samples the original's state at a VSync from emulated memory, one value
// per column of g_Th01Harness, as Th01Native_Sample does for the native
// game, from context, a TH01_DOSBOX.
void Th01Dosbox_Sample(void *context, uint32_t *row);
