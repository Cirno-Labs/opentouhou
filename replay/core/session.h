#pragma once

#include "script.h"

typedef struct {
    const char *name;
    RESULT (*apply)(uint32_t value);
} SESSION_ACTION;

// Holds the presented-frame cursor shared by retail and native drivers.
// The first outer frame-loop presentation is frame 0. Startup updates,
// calc-chain restarts and loading-screen copies inside calc stay in that
// frame. Inputs apply before calc; capture follows presentation.
typedef struct {
    INPUT_SCRIPT script;
    uint32_t frame;
    bool screenshots;
    bool fast;
    uint64_t last_present;
    const SESSION_ACTION *actions;
    size_t action_count;
    int32_t next_action;
} CAPTURE_SESSION;

// Loads an input script and starts its frame cursor at zero.
RESULT Session_Open(
    CAPTURE_SESSION *session, const char *path, const SCRIPT_BINDING *bindings,
    size_t binding_count);

// Validates every setup name before the game starts and keeps the
// bindings until Close. The game shims own their effects.
RESULT Session_SetActions(
    CAPTURE_SESSION *session, const SESSION_ACTION *actions, size_t count);

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
