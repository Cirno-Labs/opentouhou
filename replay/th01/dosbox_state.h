#pragma once

#include "harness.h"

#include "th01/frame.h"

#include <stdint.h>

// Holds the dosbox port's context: the virtual clock's VSync phase, the
// frames it began, the frame of the last OP menu call, the segment of the
// resident block once found, the run's choices as OP's options held
// them last, whether the running REIIDEN has set its cards up, and the
// last cards' count and hash.
typedef struct {
    FRAME_CLOCK clock;
    uint32_t frames;
    uint32_t menu_frame;
    uint16_t resident;
    uint32_t choices[4];
    bool cards_set;
    uint32_t card_count;
    uint32_t card_hash;
} TH01_DOSBOX;

// Samples the original's state at a VSync from emulated memory, one value
// per column of g_Th01Harness, as Th01Native_Sample does for the native
// game, from context, a TH01_DOSBOX.
void Th01Dosbox_Sample(void *context, uint32_t *row);
