#pragma once

#include "core/audio/stream.h"
#include "core/audio/types.h"
#include "core/base/result.h"
#include "core/io/reader.h"

// Decodes one audio source into float PCM on demand, compressed or raw.
// Used only by the audio engine to feed streams and samples; nothing
// outside audio/ opens a decoder directly.

typedef struct AUDIO_DECODER AUDIO_DECODER;

RESULT AudioDecoder_Open(READER *reader, AUDIO_DECODER **out);
RESULT AudioDecoder_CheckPcm(size_t size, AUDIO_PCM_FORMAT format);
RESULT AudioDecoder_OpenPcm(
    READER *reader, AUDIO_PCM_FORMAT format, AUDIO_DECODER **out);
void AudioDecoder_Free(AUDIO_DECODER *decoder);
AUDIO_INFO AudioDecoder_GetInfo(const AUDIO_DECODER *decoder);
RESULT AudioDecoder_Read(
    AUDIO_DECODER *decoder, const float **out, uint32_t *frames);
RESULT AudioDecoder_Seek(AUDIO_DECODER *decoder, uint64_t position);
