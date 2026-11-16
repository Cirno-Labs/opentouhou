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

// Marks event in the calc being recorded. Does nothing without a recording.
void Replay_MarkEvent(REPLAY_EVENT event);

// Writes the replay being played to g_GameManager.mode.replay_path again
// when the score differs from the one it holds: with the score set at the
// end of the current slot and in its header, and with new keys drawn from
// the gameplay RNG. Writes nothing when the replay was recorded with
// another config than the current one, outside practice, Extra and
// Phantasm. Then ends playback either way.
void Replay_ResaveIfScoreChanged(void);

// Returns whether the replay being played holds stage (1 to 7); false
// without playback.
bool Replay_HasStage(int32_t stage);

// Returns the event word of the frame played or recorded last in the
// stage, 0 before the first.
uint16_t Replay_GetLastEvents(void);
