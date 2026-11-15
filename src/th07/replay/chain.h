#pragma once

#include "th07/replay/types.h"

#include "core/base/result.h"

// Records and plays original-format replays, one frame per calc of a stage.
// Playback reads the replay that th07/replay/file.h keeps for its path and
// sets the buttons gameplay acts on at priority 5, with key repeat stepped
// on them. It restarts the calc chain at priority 17 on 2 calcs of 3 during
// skippable dialogue and 4 of 5 during fast playback without a boss.
// Recording marks pause events at 6 and keeps the buttons at 16.

// Starts recording or playback, or sets up the next stage of the active
// session. Playback loads path on the first registration.
RESULT Replay_Register(REPLAY_MODE mode, const char *path);

// Appends the empty frame that ends the stage being recorded.
void Replay_EndStage(void);

// Releases the session without saving.
void Replay_Discard(void);
