#pragma once

#include "core/audio/types.h"
#include "core/base/result.h"
#include "core/io/reader.h"

// Copies the path and queues loading and buffering of a paused stream.
// Performs opening, reading, decoding and format conversion on the worker.
RESULT AudioStream_LoadFile(
    const char *path, const AUDIO_PLAY_OPTIONS *options, AUDIO_STREAM **out);

// Transfers an independently owned reader on success and leaves it empty.
// Rejects borrowed buffers and slices. Prepares the paused stream on a worker.
// Requires the reader allocator to remain valid and safe for worker use
// until Free returns.
RESULT AudioStream_Load(
    READER *reader, const AUDIO_PLAY_OPTIONS *options, AUDIO_STREAM **out);

// Returns readiness without waiting. READY means initial buffering is complete.
AUDIO_LOAD_STATE AudioStream_GetState(const AUDIO_STREAM *stream);
RESULT AudioStream_GetResult(const AUDIO_STREAM *stream);
RESULT AudioStream_GetInfo(const AUDIO_STREAM *stream, AUDIO_INFO *out);

// Starts or resumes a ready stream. Reports incomplete preparation.
RESULT AudioStream_Play(AUDIO_STREAM *stream);
void AudioStream_SetPaused(AUDIO_STREAM *stream, bool paused);

// Stops at the current position. Play resumes from that position.
void AudioStream_Stop(AUDIO_STREAM *stream);
bool AudioStream_IsFinished(const AUDIO_STREAM *stream);
RESULT AudioStream_SetVolume(AUDIO_STREAM *stream, float volume);
RESULT AudioStream_SetPan(AUDIO_STREAM *stream, float pan);
// Reports rates that exceed the stream buffering capacity.
RESULT AudioStream_SetPitch(AUDIO_STREAM *stream, float pitch);

// Changes subsequent buffering without discarding audio already queued.
RESULT AudioStream_SetLoop(AUDIO_STREAM *stream, AUDIO_LOOP loop);

// Pauses playback and prepares a source position asynchronously.
// Sets LOADING until ready. Play must explicitly resume playback.
RESULT AudioStream_Seek(AUDIO_STREAM *stream, uint64_t position);

// Returns the published source sample-frame position, excluding buffered audio.
// Serves presentation and diagnostics only. Underruns retain this position.
uint64_t AudioStream_GetPosition(const AUDIO_STREAM *stream);

// Changes volume linearly over seconds of active playback.
RESULT AudioStream_Fade(
    AUDIO_STREAM *stream, float volume, double seconds, bool stop_after);

// Stops playback and waits for pending reader use to end.
// Decoder failures stop playback,
// set FAILED and remain available through GetResult until this call.
void AudioStream_Free(AUDIO_STREAM *stream);
