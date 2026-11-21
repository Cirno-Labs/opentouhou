#pragma once

#include "th01/gameplay_assets.h"
#include "th01/audio.h"
#include "th01/frame.h"
#include "th01/session.h"
#include "th01/opening.h"

typedef struct GAMEPLAY GAMEPLAY;

typedef enum {
    GAMEPLAY_REQUEST_NONE,
    GAMEPLAY_REQUEST_OPENING,
    GAMEPLAY_REQUEST_SCORE,
    GAMEPLAY_REQUEST_ENDING,
} GAMEPLAY_REQUEST;

// Prepares mutable world storage, boss snapshots, HUD and audio workspaces.
// All borrowed assets, video, session and the caller's allocator group must
// remain alive. Begin and Tick perform no allocation or IO on successful
// paths. One instance owns every ordinary stage and both boss routes.
RESULT Gameplay_Prepare(
    const ALLOCATOR *allocator, const GAMEPLAY_ASSETS *assets, VIDEO *video,
    SESSION *session, GAMEPLAY **out);

// Enters gameplay with the shared session's current credit. Reset session
// fields through Session_Begin first when entering from the opening menu.
// Resets gameplay histories while retaining prepared storage and assets.
// Runs the stage start up to its first wait within the current host tick,
// without advancing audio or VSync, as REIIDEN loads in zero time.
void Gameplay_Begin(
    GAMEPLAY *gameplay, const TH01_INPUT *input, uint64_t host_tick);

// Advances one host tick with supplied raw input and reference VSync edge.
// Advance the application's fractional frame clock across internal states.
// Nested waits suspend their owning prefix, preserving source call order.
// Terminal requests retain the current actual game frame.
GAMEPLAY_REQUEST Gameplay_Tick(
    GAMEPLAY *gameplay, const TH01_INPUT *input, bool reference_edge,
    uint64_t host_tick);

// Continues from a real score-owner handoff into the original Continue menu
// and runs it to its first wait in the score's last host tick, as REIIDEN
// goes from regist_menu into continue_menu with no VSync between. Call only
// after the score request has been handled; does no registration.
void Gameplay_ResumeAfterScore(
    GAMEPLAY *gameplay, const TH01_INPUT *input, uint64_t host_tick);
const TRAM *Gameplay_TextLayer(const GAMEPLAY *gameplay);

// Applies opening controls to the same prepared audio producer. Call the
// advance operation once per host tick outside gameplay. Gameplay_Tick
// advances audio itself, including during nested waits and terminal frames.
void Gameplay_OpeningAudio(GAMEPLAY *gameplay, OPENING_EVENTS events);
void Gameplay_AdvanceAudio(GAMEPLAY *gameplay);

// Borrows prepared output across world resets. Null selects silent output.
// Unbind before freeing output; does not change the shared producer clock.
void Gameplay_BindAudio(GAMEPLAY *gameplay, GAME_AUDIO *output);

// Borrows the shared source counters and input flags during score ownership.
// The score owner advances VSync while gameplay is suspended. Keep gameplay
// alive and resume through Gameplay_ResumeAfterScore after completion.
typedef struct {
    FRAME_VSYNC *vsync;
    GAME_INPUT *input;
} GAMEPLAY_SCORE_CONTEXT;
GAMEPLAY_SCORE_CONTEXT Gameplay_ScoreContext(GAMEPLAY *gameplay);
