#pragma once

#include "th06/replay/types.h"

#include "core/base/result.h"

#include <stdint.h>

// Runs the title and its menus: the title menu, the difficulty, character,
// shot and practice stage selects, the replay menu, the options and the
// pad button configuration, and the demo that starts after 720 calcs
// without input.
//
// The pad button configuration edits the gamepad and joystick bindings of
// shell/game and leaves the keyboard's. Each row shows one pad
// button of its action as a number, and a press of a pad button gives it
// to the row at the cursor. The d-pad, the sticks and the hats only move
// the cursor, so the rows show and take the face, shoulder, back, start,
// guide and stick-press buttons and the joystick buttons. Leaving the
// screen asks shell/game for the changed bindings. The original's defaults
// become the port's default pad bindings. The window row of the options
// shows and changes shell/game's window mode, which changes when the options
// close. The colour depth row shows no choice and ignores changes (see
// th06/settings.h).
//
// The original measures the frame rate on the title to slow the game down
// on fast displays, and waits until three seconds after the start before
// it plays the title music. The port does neither.

// Names the replay that the demo plays. Playback reads it from th06/replay's
// store, so the program keeps it there before the first tick.
#define TITLE_DEMO_PATH "data/demo/demo00.rpy"

// Adds the title at calc priority 2 and draw priority 0. Opens the replay
// menu when mode is nonzero, or the title otherwise. The first calc loads
// the files of the screen it opens.
//
// Opening the replay menu asks for a scan of the replay directory
// (th06/screens/result/saves.h), which runs after that tick. From the title
// menu, the list first shows at the 60th calc after the choice, with the
// scanned files. Opened after a replay, the list shows at the first calc
// with the files of the previous scan, and the scanned files from the
// second calc on.
RESULT TitleScene_SetUp(int32_t mode);
// Sets g_GameManager up to play the replay at path from stage, from 0 for
// stage 1, as the replay menu does, and requests SCENE_GAMEPLAY. The lives
// and bombs come from the first stage of the replay, not from stage.
// Reports failure when path does not fit g_GameManager.
RESULT TitleScene_SetUpReplay(
    const REPLAY_SUMMARY *summary, const char *path, int32_t stage);
// Removes the title, as leaving it for a scene does. Does nothing while the
// title is not added.
void TitleScene_Remove(void);
