#pragma once

#include "core/base/result.h"

// Opens stereo audio output and starts background decoding.
// Runs public audio calls on the main thread. Never query audio from
// simulation.
RESULT Audio_Init(void);

// Stops output, cancels jobs and joins the decoder before releasing resources.
// Invalidates all sample, stream and playback pointers.
void Audio_Shutdown(void);

// Returns whether Audio_Init succeeded and Audio_Shutdown has not run since.
bool Audio_IsInitialized(void);
