#pragma once

#include "core/base/result.h"
#include "core/base/types.h"

#define AUDIO_OUTPUT_RATE 48000

RESULT AudioBackend_Init(void);
void AudioBackend_Shutdown(void);
bool AudioBackend_Mix(float *out, uint32_t frames);
