#pragma once

#include "th01/gameplay_assets.h"
#include "th01/vsync_irq.h"
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

// Enters gameplay with the session's current credit and runs the stage start
// to its first wait; after the opening menu, call Session_Begin first.
void Gameplay_Begin(const TH01_INPUT *input, uint64_t host_tick);

// Advances one host tick with raw input and the reference VSync edge.
// A program switch first fades to black over sixteen VSyncs, counters stopped.
GAMEPLAY_REQUEST Gameplay_Tick(
    const TH01_INPUT *input, bool reference_edge, uint64_t host_tick);

// Enters the Continue menu from a handled score request, within the score's
// last host tick. Without fix_debouncing, the held name confirm answers Yes.
void Gameplay_ResumeAfterScore(const TH01_INPUT *input, uint64_t host_tick);

void Gameplay_Close(void);
