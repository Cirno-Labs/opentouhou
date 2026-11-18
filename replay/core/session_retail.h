#pragma once

#include "script.h"

#include <windows.h>

// Loads CAPTURE_INPUTS and shares the native driver's input/capture protocol.
// The launcher must already have installed the write overlay and clocks.
RESULT SessionRetail_Open(const SCRIPT_BINDING *bindings, size_t count);

// Hooks the device's presentation on the first calc. device is the game's
// IDirect3DDevice8, kept opaque outside this Windows-only implementation.
void SessionRetail_BeforeCalc(void *device);

// Ends the outer calc. Presents inside a calc, such as loading-screen
// copies, do not advance the input cursor or request a capture.
void SessionRetail_AfterCalc(void);

// Supplies the current frame's input to the original ReadButtons call sites.
uint16_t __cdecl SessionRetail_Buttons(void);
