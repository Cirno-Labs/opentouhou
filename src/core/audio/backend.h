#pragma once

#include "core/base/result.h"
#include "core/base/types.h"

// Drives the OS audio output device. A platform backend implements this
// one entry point by calling back into the mixer each buffer; everything
// above it in audio/ is platform-independent.

#define AUDIO_OUTPUT_RATE 48000

RESULT AudioBackend_Init(void);
void AudioBackend_Shutdown(void);
bool AudioBackend_Mix(float *out, uint32_t frames);
