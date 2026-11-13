// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/audio/types.h"
#include "core/base/result.h"
#include "core/io/reader.h"

typedef struct AUDIO_DECODER AUDIO_DECODER;

RESULT AudioDecoder_Open(READER *reader, AUDIO_DECODER **out);
void AudioDecoder_Free(AUDIO_DECODER *decoder);
AUDIO_INFO AudioDecoder_GetInfo(const AUDIO_DECODER *decoder);
RESULT AudioDecoder_Read(
    AUDIO_DECODER *decoder, const float **out, uint32_t *frames);
RESULT AudioDecoder_Seek(AUDIO_DECODER *decoder, uint64_t position);
