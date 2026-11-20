#pragma once

#include "th07/replay/types.h"

#include "core/base/result.h"

#include <stdint.h>

// Runs the title and its menus: the title menu, the difficulty, character,
// shot and practice stage selects, the replay menu, the options and the
// pad button configuration, and the demo that starts after 900 calcs
// without input. When Phantasm opens, the title first shows its splash for
// 900 calcs, which shoot, bomb or confirm cut short from the 120th.
//
// The pad button configuration edits the gamepad and joystick bindings of
// shell/game and leaves the keyboard's. Each row shows one pad button of
// its action as a number, and a press of a pad button gives it to the row
// at the cursor; every other row that had it takes the row's old button.
// The d-pad, the sticks and the hats only move the cursor. Leaving the
// screen asks shell/game for the changed bindings. The window row of the
// options shows and changes shell/game's window mode, which changes when
// the options close. The color depth row shows no choice and ignores
// changes (see th07/settings.h).
//
// The original shows a loading screen when it comes back to the title.
// The port does not.

// Counts the demo replays, which the title plays in turn. Playback reads
// them from th07/replay's store, so the program keeps them there before
// the first tick.
#define TITLE_DEMO_COUNT 3

extern const char *const g_DemoReplayPaths[TITLE_DEMO_COUNT];

// Adds the title at calc priority 3 and draw priority 0. The title opens
// the replay menu after a replay, and the title menu otherwise; mode is
// unused, as in the original.
//
// The replay menu asks for a scan of the replay directory
// (th07/screens/result/saves.h) as it opens, which runs after that tick.
// The menu shows the files of the previous scan until then.
RESULT TitleScene_SetUp(int32_t mode);

// Sets g_GameManager up to play the replay at path from stage, from 0 for
// stage 1, as the replay menu does, and requests SCENE_GAMEPLAY. A
// Phantasm replay always starts at stage 8. playback is the replay menu's
// choice: 0 normal, 1 slow, 2 fast. Reports failure when path does not fit
// g_GameManager.
RESULT TitleScene_SetUpReplay(
    const REPLAY_SUMMARY *summary, const char *path, int32_t stage,
    int32_t playback);

// Returns whether the title rests at the title menu, ready for input, or
// at the replay menu.
bool TitleScene_IsIdle(void);

// Removes the title, as leaving it for a scene does. Does nothing while the
// title is not added.
void TitleScene_Remove(void);
