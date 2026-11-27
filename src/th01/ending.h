#pragma once

#include "th01/score.h"
#include "th01/session.h"

#define ENDING_SHEET_COUNT 15

typedef struct {
    uint32_t score;
    uint32_t highest_score;
    uint32_t random;
    uint32_t continues[4];
    uint32_t total;
    int32_t skill;
    int32_t group;
    int32_t level;
    int8_t extra_lives;
    uint8_t rank;
    SESSION_END route;
} END_RESULT;

typedef struct {
    PI_IMAGE sheets[ENDING_SHEET_COUNT];
    GAME_TEXT_FONT font;
} ENDING_ASSETS;

extern const END_RESULT *const g_EndResult;

// Snapshots the run's score and histories, then clears the shared run
// history before the story. Requires a completed route.
void Ending_Snapshot(END_RESULT *result);

// Calculates the signed source thresholds and advances its 32-bit LCG once.
void Ending_CalculateTitle(END_RESULT *result);

// Clears ending history and stage after registration, retaining settings,
// live resources, route, random checkpoints and cached table leader.
void Ending_Clear(void);

// Loads all ending sheets, the score background and glyphs from rom.
// Discard the allocator group on failure.
RESULT Ending_LoadAssets(
    VFS *vfs, const FONT_ROM *rom, const ALLOCATOR *allocator,
    ENDING_ASSETS *out);

// Borrows the loaded assets through Close without allocating scene storage.
void Ending_Init(const ENDING_ASSETS *assets);

// Snapshots and clears shared history, then starts the real story. g_VSync
// and g_GameInput stay shared through results and score registration.
void Ending_Begin(void);

// Advances VSync once and runs the ending until a wait suspends it. Returns
// true when the results screen hands its background to score.
bool Ending_Tick(const TH01_INPUT *input, bool reference_edge);

// Starts the program switch after registration, fading the palette to black
// over sixteen VSyncs with the counters stopped.
void Ending_BeginSwitch(void);

// Advances the switch by one host tick. Returns true after the last step,
// when the switch to OP follows at once.
bool Ending_SwitchTick(bool reference_edge);

void Ending_Close(void);
