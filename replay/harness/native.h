#pragma once

#include "replay/harness/backend.h"

#include "shell/game/game.h"

// Runs a native game under the driver. The backend program calls
// Native_Main, which starts the game with the staged files
// (Driver_MountStaged) over its data files and its user files, work/
// (Driver_MountWork) over its user files only, no sound and no patches, then
// runs one tick per frame without interpolation until Driver_Done. It stands in
// for shell/game with shell/null.h: each tick's input holds the bindings of
// Driver_Buttons, the tick rate is what Driver_Fps returns, and the wall clock
// counts the frames' milliseconds, rounded up. A tick draws only when
// Driver_WantsDraw, and a headless run opens no video. Text uses the
// built-in fonts alone, never the system's, so that runs draw alike.

typedef struct {
    PORT port;
    const GAME_DESC *game;
    // Changes the game's settings after Native_Main sets their defaults and
    // before the game starts, or nullptr.
    void (*configure)(void *context);
    // Fills in frame metadata (date, clock samples) that the game's own
    // input schema does not cover, right before each tick, or nullptr.
    void (*prepare_input)(void *context, DRIVER *driver, void *input);
    // Gives the port the driver before each tick, for the port to call
    // Driver_EndCalc after each calc of that tick.
    void (*hook)(void *context, DRIVER *driver);
} NATIVE_PORT;

// Runs the command line of Run_Parse and returns the exit status: 0 when
// the run matches, 1 when it differs, 2 on an error.
int Native_Main(const NATIVE_PORT *port, int argc, char **argv);
