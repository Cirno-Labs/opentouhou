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

typedef enum {
    ENDING_AUDIO_IRIS,
    ENDING_AUDIO_FADE,
    ENDING_AUDIO_STOP,
    ENDING_AUDIO_STAGE1,
} ENDING_AUDIO;

typedef void (*ENDING_AUDIO_CONSUMER)(void *context, ENDING_AUDIO command);
typedef struct ENDING ENDING;

// Snapshots signed score bits, zero-extended scene histories and the last
// life-exit random checkpoint. Clears shared run history before the story.
// Requires a completed route. The result owns its copied values.
void Ending_Snapshot(END_RESULT *result, SESSION *session);

// Calculates the signed source thresholds and advances its 32-bit LCG once.
void Ending_CalculateTitle(END_RESULT *result);

// Clears ending history and stage after registration, retaining settings,
// live resources, route, random checkpoints and cached table leader.
void Ending_Clear(SESSION *session);

// Loads all ending sheets, cleared score background and text glyphs before
// ticks. Discard the allocator group on failure. No original data is retained
// outside the caller's assets. Uses the bundled font as a ROM substitute.
RESULT Ending_LoadAssets(
    VFS *vfs, const ALLOCATOR *allocator, ENDING_ASSETS *out);

// Reserves the owner and its complete authored operation workspace. Audio
// consumption is mandatory and must use prepared simulation controls.
void Ending_Prepare(
    const ALLOCATOR *allocator, const ENDING_ASSETS *assets, VIDEO *video,
    FRAME_VSYNC *vsync, GAME_INPUT *input, ENDING_AUDIO_CONSUMER audio,
    void *audio_context, ENDING **out);

// Snapshots and clears shared history, then starts the real story. Borrowed
// counters and input remain live through results and score registration.
void Ending_Begin(ENDING *ending, SESSION *session);

// Advances VSync once and executes source requests until a wait suspends.
// Returns true when the results screen hands its retained background to score.
// Advance audio once per host tick outside this operation. No allocation,
// IO, device or clock reads occur in Begin or Tick. Every ending delay
// resets count1 at entry, including zero durations, and retains it on exit.
bool Ending_Tick(ENDING *ending, const TH01_INPUT *input, bool reference_edge);
const END_RESULT *Ending_Result(const ENDING *ending);
