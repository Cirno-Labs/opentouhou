#pragma once

#include "th01/frame.h"
#include "th01/palette.h"

typedef struct {
    uint16_t transferred[VIDEO_HEIGHT];
    bool active[VIDEO_HEIGHT];
    uint16_t activated, done, previous_done;
    bool pending, waiting;
} BONUS_ROW_TRANSITION;

// Shifts page 0 by 32 pixels, left on even rows and right on odd rows.
// Injects page 1 chunks in forward or reverse order. Requires y 0..399 and
// transferred 0..76 in bytes, divisible by four. Selects drawing page 0.
void Bonus_RowShift32(VIDEO *video, uint16_t y, uint16_t transferred);

// Starts with the source's empty scan and two-frame delay. Stores all row
// histories in caller-owned bounded state. Keep page 1 and target live until
// completion. Resume uses count1, resets it only when each delay succeeds,
// and includes the last delay. Pending polls do not repeat shifts or fades.
// Does not advance VSync, allocate, perform IO or read external state.
void Bonus_BeginRowTransition(BONUS_ROW_TRANSITION *transition);
bool Bonus_ResumeRowTransition(
    BONUS_ROW_TRANSITION *transition, VIDEO *video, PALETTE_STATE *palette,
    const PALETTE *target, FRAME_VSYNC *vsync);
