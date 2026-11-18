#pragma once

#include "core/base/result.h"

#include <stddef.h>
#include <stdint.h>

// Runs what every game's capture DLL shares: the overlay, the clocks, the
// null renderer, the case file and closing the game.

typedef enum {
    CAPTURE_LIVE,
    CAPTURE_REPLAY,
    CAPTURE_RESULT,
    CAPTURE_SCRIPT,
} CAPTURE_MODE;

typedef struct {
    // "th07".
    const char *game_id;
    // "th07 1.00b".
    const char *info;
    const char *const *start_names;
    size_t start_count;
    const char *const *calc_names;
    size_t calc_count;
    void (*start)(uint32_t *row);
    void (*calc)(uint32_t *row);
    // Logs the closed case, or nullptr to log only its row count.
    void (*closed)(uint32_t rows);
} CAPTURE_CASE;

// Installs the overlay, the clocks and the null renderer as the launcher
// asked, and starts the --quit-after timer. Ends the process on failure.
// Returns the mode; replay gets the replay's path in CAPTURE_REPLAY.
CAPTURE_MODE Capture_Attach(
    const CAPTURE_CASE *capture, char *replay, size_t size);

// Ends the process unless result is OK.
void Capture_Check(RESULT result);

// Counts a calc; logs every 600.
void Capture_CountCalc(void);

// Opens the case file once, or closes it; both do nothing when already so.
void Capture_OpenCase(void);
void Capture_CloseCase(void);

// Writes the calc's row while the case is open.
void Capture_WriteRow(void);

// Counts frames on the title; true on the frame where the replay or the
// result screen starts.
bool Capture_TitleDelayReached(bool on_title);

// Asks the game's window to close.
void Capture_CloseGame(void);
