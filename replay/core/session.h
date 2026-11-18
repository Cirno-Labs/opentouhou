#pragma once

#include "script.h"

// Holds the presented-frame cursor shared by retail and native drivers.
// The first draw/calc/present cycle is frame 0. A calc-chain restart stays
// in that frame. Inputs apply before calc; capture follows presentation.
typedef struct {
    INPUT_SCRIPT script;
    uint32_t frame;
} CAPTURE_SESSION;

// Loads an input script and starts its frame cursor at zero.
RESULT Session_Open(
    CAPTURE_SESSION *session, const char *path, const SCRIPT_BINDING *bindings,
    size_t binding_count);

// Requests an actual window capture on stdout as
// "SESSION capture FRAME X Y WIDTH HEIGHT", then waits for the controller's
// "CAPTURED FRAME" on stdin. Advances only after acknowledgement, or at
// once when no capture was requested. The controller owns the screenshots.
RESULT Session_Presented(
    CAPTURE_SESSION *session, int x, int y, int width, int height);

// Returns whether every scripted frame has been presented.
bool Session_Done(const CAPTURE_SESSION *session);

// Releases the script.
void Session_Close(CAPTURE_SESSION *session);
