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

#include "th01/extend.h"
#include "th01/input.h"
#include "th01/timer.h"
#include "th01/reiiden_image.h"

typedef struct {
    uint8_t box[4];
} BONUS_STAGE_ASSETS;

typedef enum {
    BONUS_DONE,
    BONUS_STAGE_BOX,
    BONUS_CONFIRM,
} BONUS_PHASE;

typedef struct {
    BONUS_PHASE phase;
    int16_t stage_number, row;
    bool waiting;
} BONUS;

typedef struct {
    const BONUS_STAGE_ASSETS *stage;
    const GAME_TEXT_FONT *font;
    const PALETTE *stage_palette;
    PALETTE_STATE *palette;
    HUD *hud;
    const HUD_CONTEXT *display;
    const EXTEND_CONTEXT *extend;
    const STAGE_TIMER *timer;
    GAME_INPUT *input;
    uint32_t *score_bonus;
    int32_t *bonus_per_stage;
} BONUS_CONTEXT;

// Loads the stage box mask from the supported REIIDEN.EXE into caller asset
// storage. Requires REIIDEN_IMAGE_SIZE scratch bytes; keeps reader open and
// leaves out null on error. Scratch is not retained.
RESULT Bonus_LoadStageAssets(
    READER *reader, uint8_t *scratch, size_t scratch_size,
    const ALLOCATOR *allocator, BONUS_STAGE_ASSETS **out);

// Caches stage labels and fullwidth numerals with the portable font substitute.
// Does not claim the PC-98 ROM's glyph shapes.
RESULT Bonus_LoadFont(
    FONT *font, const ALLOCATOR *allocator, GAME_TEXT_FONT *out);
RESULT Bonus_ValidateStageAssets(
    const BONUS_STAGE_ASSETS *assets, const GAME_TEXT_FONT *font);

// Starts an ordinary stage 1..20, excluding multiples of five. Preserves the
// current drawing page while opening the masked box. Reads live metrics after
// opening, adds to the existing unsigned score_bonus, writes one of four
// signed resident history slots with the score bits, then calls the real HUD
// and extend consumers. Keep all bindings and loaded resources stable through
// completion. Zero-initialize BONUS once; begin only when done.
// Wrapped negative display digits draw blank rather than reading outside the
// original numeral table; score arithmetic still retains the source widths.
bool Bonus_BeginStage(
    BONUS *bonus, const BONUS_CONTEXT *context, int16_t stage_number,
    const TH01_INPUT *raw, FRAME_VSYNC *vsync);

// Resumes source-order waits without advancing VSync or gameplay. Confirmation
// resets real GameInput history, forces shot and OK true and senses until
// either becomes false. A held or newly pressed key exits on release, not
// press. Idle input does not end the wait. Preserves bomb_frame and clears
// score_bonus only on confirmation. Updates and pending polls allocate nothing,
// perform no IO and read no clock, font handle or external device.
bool Bonus_Resume(
    BONUS *bonus, const BONUS_CONTEXT *context, const TH01_INPUT *raw,
    FRAME_VSYNC *vsync);
