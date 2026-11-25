#pragma once

#include "th01/score.h"
#include "th01/session.h"

#define ENDING_SHEET_COUNT 15

typedef struct {
    uint32_t score, highest_score, random;
    uint32_t continues[4], total;
    int32_t skill, group, level;
    int8_t extra_lives;
    uint8_t rank;
    SESSION_END route;
} END_RESULT;

typedef struct {
    PI_IMAGE sheets[ENDING_SHEET_COUNT];
    GAME_TEXT_FONT font;
} ENDING_ASSETS;

extern const END_RESULT *const g_EndResult;

// Snapshots signed score bits, zero-extended scene histories and the last
// life-exit random checkpoint. Clears shared run history before the story.
// Requires a completed route. The result owns its copied values.
void Ending_Snapshot(END_RESULT *result);

// Calculates the signed source thresholds and advances its 32-bit LCG once.
void Ending_CalculateTitle(END_RESULT *result);

// Clears ending history and stage after registration, retaining settings,
// live resources, route, random checkpoints and cached table leader.
void Ending_Clear(void);

// Loads all ending sheets, cleared score background and text glyphs from rom
// before ticks. Discard the allocator group on failure. No original data is
// retained outside the caller's assets.
RESULT Ending_LoadAssets(
    VFS *vfs, const FONT_ROM *rom, const ALLOCATOR *allocator,
    ENDING_ASSETS *out);

// Borrows the loaded assets through Close without allocating scene storage.
void Ending_Init(const ENDING_ASSETS *assets);

// Snapshots and clears shared history, then starts the real story. g_VSync
// and g_GameInput stay shared through results and score registration.
void Ending_Begin(void);

// Advances VSync once and executes source requests until a wait suspends.
// Returns true when the results screen hands its retained background to score.
// Advance audio once per host tick outside this operation. No allocation,
// IO, device or clock reads occur in Begin or Tick. Every ending delay
// resets count1 at entry, including zero durations, and retains it on exit.
bool Ending_Tick(const TH01_INPUT *input, bool reference_edge);

// Starts game_switch_binary after the results screen's registration: its
// vsync_exit stops the counters, and its z_palette_black_out steps the
// ending's palette to black after each of sixteen VSyncs.
void Ending_BeginSwitch(void);

// Advances the switch by one host tick. Returns true after the last step,
// when the switch to OP follows at once.
bool Ending_SwitchTick(bool reference_edge);

void Ending_Close(void);
