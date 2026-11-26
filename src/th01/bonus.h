#pragma once

#include "th01/extend.h"
#include "th01/vsync_irq.h"
#include "th01/input.h"
#include "th01/palette.h"
#include "pc98/zun/pi.h"
#include "th01/timer.h"

#include "core/vfs.h"

typedef struct {
    uint8_t box[4];
} BONUS_STAGE_ASSETS;

typedef struct {
    PI_IMAGE background;
    PTN numerals;
    PALETTE palette;
} BONUS_TOTAL_ASSETS;

// Shifts page 0 by 32 pixels, left on even rows and right on odd rows,
// feeding in page 1. transferred is 0..76 bytes, divisible by four.
void Bonus_RowShift32(uint16_t y, uint16_t transferred);

void Bonus_InitRowTransition(void);

// Starts with the empty scan and its two-count wait.
void Bonus_BeginRowTransition(void);

// Includes the final two-count delay, clearing count1 only after each delay
// succeeds.
bool Bonus_ResumeRowTransition(PALETTE_STATE *palette, const PALETTE *target);

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

// Starts the bonus for ordinary stage 1..20, not a multiple of five.
// Keep the bindings and resources stable until done.
bool Bonus_BeginStage(
    const BONUS_STAGE_ASSETS *stage, const GAME_TEXT_FONT *font,
    int16_t stage_number, const TH01_INPUT *raw);

// Resumes the bonus screen; returns true when the player confirms.
// Confirmation ends on key release, not press.
bool Bonus_Resume(
    const BONUS_STAGE_ASSETS *stage, const GAME_TEXT_FONT *font,
    const TH01_INPUT *raw);

// Loads CLEAR3.GRP and NUMB.PTN into the caller's grouped allocator.
RESULT Bonus_LoadTotalAssets(
    VFS *vfs, const ALLOCATOR *allocator, BONUS_TOTAL_ASSETS *out);
RESULT Bonus_ValidateTotalAssets(const BONUS_TOTAL_ASSETS *assets);

void Bonus_InitTotal(void);

// Starts the boss-stage total for stage 5, 10, 15 or 20.
// Unlike the stage bonus, does not update the HUD or the extend threshold.
bool Bonus_BeginTotal(
    const BONUS_TOTAL_ASSETS *assets, int16_t stage_number,
    const TH01_INPUT *raw, uint64_t host_tick);

// Resumes the total screen; returns true when the player confirms.
// Advance only VSync and audio while the world is suspended.
bool Bonus_ResumeTotal(
    const BONUS_TOTAL_ASSETS *assets, const TH01_INPUT *raw,
    uint64_t host_tick);
