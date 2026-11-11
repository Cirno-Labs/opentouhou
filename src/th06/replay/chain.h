#pragma once

#include "th06/replay/types.h"

#include "core/io/writer.h"

// Records and plays original-format replays. Playback reads the replay
// that th06/replay/file.h keeps for its path. Playback restores each stage
// before gameplay starts and replaces its gameplay buttons at priority 5.
// Skippable dialogue restarts the calc chain at priority 16. Recording
// samples button changes at priority 15. Dates and frame-rate totals for
// saving come from outside the simulation.

// Starts recording or playback, or initializes the next stage of the
// active session. Playback loads path on the first registration.
RESULT Replay_Register(REPLAY_MODE mode, const char *path);
// Appends the stage's terminal entries in recording and playback.
void Replay_EndStage(void);
// Finalizes and writes the recording, then releases the session. A
// playback session is released without writing. Leaves writer open.
// Invalid save metadata leaves the recording active. A write failure
// releases it after reporting failure. Draws four values from the gameplay
// RNG when writing a recording.
RESULT Replay_Save(WRITER *writer, const REPLAY_SAVE_INFO *info);
// Releases the session without saving.
void Replay_Discard(void);
