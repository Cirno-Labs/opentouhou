#pragma once

#include "th07/replay/types.h"

#include "core/base/result.h"

// Records and plays original-format replays, one frame per calc of a stage.
// Playback reads the replay that th07/replay/file.h keeps for its path and
// sets the buttons gameplay acts on at priority 5, with key repeat stepped
// on them. It restarts the calc chain at priority 17 on 2 calcs of 3 during
// skippable dialogue and 4 of 5 during fast playback without a boss.
// Recording marks pause events at 6 and keeps the buttons at 16.
// With [fixes] input_lag enabled, gameplay outside playback acts on the
// buttons in the calc that reads them, also while paused, and recording
// skips each stage's first calc, as VsyncPatch rev4 makes the original do.
// With [fixes] replay_desync enabled, playback keeps stage headers intact.
// In a recording whose bombs start on the frame of their press, a frame
// before the original records them, as the original with VsyncPatch rev4
// records them, it also takes the buttons on a pause marker as held the
// calc before, since that game reads the buttons while paused. Recording
// is unchanged. With the fix disabled, playback is retail's.

// Starts recording or playback, or sets up the next stage of the active
// session. Playback loads path and takes the replay_desync setting on the
// first registration; the setting stays for the whole playback.
RESULT Replay_Register(REPLAY_MODE mode, const char *path);

// Appends the empty frame that ends a recorded stage. During playback,
// does nothing with replay_desync enabled. With it disabled, clears the
// next input in the loaded file too; a stage that runs a calc late can
// clear the low word of the next stage's saved score.
void Replay_EndStage(void);

// Ends the stage being recorded and writes the recording to path, as
// g_GameManager.mode.replay_path names it, with the score and info, through
// ResultScreen_PutReplayFile. Draws three values from the gameplay RNG when
// it writes. Writes nothing for a recording in slow mode, or for one made
// with another config than the current one outside practice, Extra and
// Phantasm. Then releases the session, also a playback session.
void Replay_Save(const char *path, const REPLAY_SAVE_INFO *info);

// Releases the session without saving.
void Replay_Discard(void);

// Marks event in the calc being recorded. Does nothing without a recording.
void Replay_MarkEvent(REPLAY_EVENT event);

// Writes the replay being played to g_GameManager.mode.replay_path again
// when the score differs from the one it holds: with the score set at the
// end of the current slot and in its header, and with new keys drawn from
// the gameplay RNG. Writes nothing when the replay was recorded with
// another config than the current one, outside practice, Extra and
// Phantasm, or when replay_desync is enabled. Then ends playback either way.
void Replay_ResaveIfScoreChanged(void);

// With [fixes] input_lag enabled, sets the play words from the live word
// outside replay playback. Called after each calc reads the buttons, also
// while the game is paused.
void Replay_TakeLiveButtons(void);

// Returns whether the replay being played holds stage (1 to 7); false
// without playback.
bool Replay_HasStage(int32_t stage);

// Returns the event word of the frame played or recorded last in the
// stage, 0 before the first.
uint16_t Replay_GetLastEvents(void);
