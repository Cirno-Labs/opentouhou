#pragma once

#include "th01/extend.h"
#include "th01/vsync_irq.h"
#include "th01/input.h"
#include "th01/palette.h"
#include "pc98/zun/pi.h"
#include "th01/timer.h"

#include "core/vfs.h"

// Shifts page 0 by 32 pixels, left on even rows and right on odd rows.
// Injects page 1 chunks in forward or reverse order. Requires y 0..399 and
// transferred 0..76 in bytes, divisible by four. Selects drawing page 0.
void Bonus_RowShift32(uint16_t y, uint16_t transferred);

void Bonus_InitRowTransition(void);

// Starts with the empty scan and its two-count wait.
void Bonus_BeginRowTransition(void);

// Includes the final two-count delay, clearing count1 only after each delay
// succeeds.
bool Bonus_ResumeRowTransition(PALETTE_STATE *palette, const PALETTE *target);

typedef struct {
    uint8_t box[4];
} BONUS_STAGE_ASSETS;

// Clears stage and total bonus state for a new gameplay session.
void Bonus_Init(void);

// Loads the stage box mask from bonusbox.bmp into caller asset storage;
// leaves out null on error.
RESULT Bonus_LoadStageAssets(
    VFS *vfs, const ALLOCATOR *allocator, BONUS_STAGE_ASSETS **out);

// Caches stage labels and fullwidth numerals from rom.
RESULT Bonus_LoadFont(
    const FONT_ROM *rom, const ALLOCATOR *allocator, GAME_TEXT_FONT *out);
RESULT Bonus_ValidateStageAssets(
    const BONUS_STAGE_ASSETS *assets, const GAME_TEXT_FONT *font);

// Starts an ordinary stage 1..20, excluding multiples of five. Preserves the
// current drawing page while opening the masked box. Reads live metrics after
// opening, adds to the existing unsigned score_bonus, writes one of four
// signed resident history slots with the score bits, then draws the HUD and
// checks for an extend. Keep all bindings and loaded resources stable through
// completion. Begin only when done.
// Wrapped negative display digits draw blank rather than reading outside the
// original numeral table; score arithmetic still retains the source widths.
bool Bonus_BeginStage(
    const BONUS_STAGE_ASSETS *stage, const GAME_TEXT_FONT *font,
    int16_t stage_number, const TH01_INPUT *raw);

// Resumes source-order waits without advancing VSync or gameplay. Confirmation
// resets real GameInput history, forces shot and OK true and senses until
// either becomes false. A held or newly pressed key exits on release, not
// press. Idle input does not end the wait. Preserves bomb_frame and clears
// score_bonus only on confirmation. Updates and pending polls allocate nothing,
// perform no IO and read no clock, font handle or external device.
bool Bonus_Resume(
    const BONUS_STAGE_ASSETS *stage, const GAME_TEXT_FONT *font,
    const TH01_INPUT *raw);

typedef struct {
    PI_IMAGE background;
    PTN numerals;
    PALETTE palette;
} BONUS_TOTAL_ASSETS;

// Loads CLEAR3.GRP and NUMB.PTN into the caller's grouped allocator before
// simulation. Keeps no file handles; failed loads leave out empty and any
// intermediate allocations in that group. No individual resource frees occur.
RESULT Bonus_LoadTotalAssets(
    VFS *vfs, const ALLOCATOR *allocator, BONUS_TOTAL_ASSETS *out);
RESULT Bonus_ValidateTotalAssets(const BONUS_TOTAL_ASSETS *assets);

void Bonus_InitTotal(void);

// Starts a boss-stage total for stage 5, 10, 15 or 20. Uses real shot and
// pellet reset operations before the initial fifty-frame wait. Page 1 becomes
// the cached total background, then alternating rows transfer it to page 0.
// Awards the score after all four metrics, before the six-digit sum. Total
// does not update the HUD or extend threshold. Saves the low sixteen bits of
// live continues_total after resetting input, before the confirmation loop.
bool Bonus_BeginTotal(
    const BONUS_TOTAL_ASSETS *assets, int16_t stage_number,
    const TH01_INPUT *raw, uint64_t host_tick);

// Resumes source frame delays with count1 carry and four-millisecond digit
// delays with fractional credit from a monotonic 60 Hz host serial. Several
// digit steps can execute in one host tick. The digit delays start at the
// VSync that ends the stage number's frame delay, with the rest of that
// tick as credit, and keep their credit through the metrics' frame delays,
// which pass at once because each metric's delays take longer. Visibility
// is host-quantized; CPU drawing cost and DOS delay calibration are not
// modeled. Numeral spins retain the source's scrambled second loop,
// including its two blank quarters.
// Final digits outside the numeral table draw blank, preserving arithmetic
// without reproducing undefined source table reads.
// Uses the same release confirmation and bounded caller storage as stage
// bonuses. Advance only independent VSync/audio while the world is suspended.
// Successful updates and pending polls allocate nothing and perform no IO.
bool Bonus_ResumeTotal(
    const BONUS_TOTAL_ASSETS *assets, const TH01_INPUT *raw,
    uint64_t host_tick);
