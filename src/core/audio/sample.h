#pragma once

#include "core/audio/types.h"
#include "core/base/result.h"
#include "core/io/reader.h"

// Copies the UTF-8 path and queues opening, reading and decoding on a worker.
// Returns whether the request was accepted, before any audio is decoded.
RESULT AudioSample_LoadFile(const char *path, AUDIO_SAMPLE **out);

// Transfers an independently owned reader on success and leaves it empty.
// Rejects borrowed buffers and slices. Reads and decodes on a worker.
// Requires the reader allocator to remain valid and safe for worker use
// until loading completes or Free returns.
RESULT AudioSample_Load(READER *reader, AUDIO_SAMPLE **out);

// Returns readiness without waiting. READY means the entire sample is decoded.
AUDIO_LOAD_STATE AudioSample_GetState(const AUDIO_SAMPLE *sample);

// Returns OK after loading succeeds, a new copy of its failure after failure,
// or an incomplete-loading failure while loading continues.
RESULT AudioSample_GetResult(const AUDIO_SAMPLE *sample);

// Returns source metadata after loading succeeds.
RESULT AudioSample_GetInfo(const AUDIO_SAMPLE *sample, AUDIO_INFO *out);

// Cancels unfinished loading and releases the sample pointer.
// Waits for pending reader use to end. Existing playbacks retain decoded data.
void AudioSample_Free(AUDIO_SAMPLE *sample);
