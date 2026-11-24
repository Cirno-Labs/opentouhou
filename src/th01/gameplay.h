#pragma once

#include "th01/gameplay_assets.h"
#include "th01/frame.h"
#include "th01/session.h"
#include "th01/input.h"

typedef enum {
    GAMEPLAY_REQUEST_NONE,
    GAMEPLAY_REQUEST_OPENING,
    GAMEPLAY_REQUEST_SCORE,
    GAMEPLAY_REQUEST_ENDING,
} GAMEPLAY_REQUEST;

// Borrows loaded assets through Close and allocates only during initialization.
void Gameplay_Init(const ALLOCATOR *allocator, const GAMEPLAY_ASSETS *assets);

// Restores fresh gameplay defaults without reloading assets or output.
void Gameplay_Reset(void);

// Enters gameplay with the shared session's current credit. Reset session
// fields through Session_Begin first when entering from the opening menu.
// Resets gameplay histories while retaining prepared storage and assets.
// Runs the stage start up to its first wait within the current host tick,
// without advancing audio or VSync, as REIIDEN loads in zero time.
void Gameplay_Begin(const TH01_INPUT *input, uint64_t host_tick);

// Advances one host tick with supplied raw input and reference VSync edge.
// Advance the application's fractional frame clock across internal states.
// Nested waits suspend their owning prefix, preserving source call order.
// Terminal requests retain the current actual game frame. A program switch
// (into or out of a boss stage, Continue, the title, the ending) first
// blacks the palette out after each of sixteen VSyncs with the counters
// stopped, as game_switch_binary does, then requests or resets at once.
GAMEPLAY_REQUEST Gameplay_Tick(
    const TH01_INPUT *input, bool reference_edge, uint64_t host_tick);

// Continues from a real score-owner handoff into the original Continue menu
// and runs it to its first wait in the score's last host tick, as REIIDEN
// goes from regist_menu into continue_menu with no VSync between. In the
// original the press that confirms the name is still down there and
// confirms Yes; fix_debouncing makes the menu wait for a fresh press. Call
// only after the score request has been handled; does no registration.
void Gameplay_ResumeAfterScore(const TH01_INPUT *input, uint64_t host_tick);

void Gameplay_Close(void);
