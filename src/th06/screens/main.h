#pragma once

#include "th06/replay/types.h"

#include "core/base/result.h"

#include <stdint.h>

// Runs the title and its menus: the title menu, the difficulty, character,
// shot and practice stage selects, the replay menu, the options and the
// pad button configuration, and the demo that starts after 720 calcs
// without input.
//
// The port reads no raw pad buttons, so the pad button configuration
// shows and edits the original's mapping but can assign a button only by
// restoring the defaults, and the mapping has no effect on the input.
// The options show no choice for the colour depth and the window mode,
// which the port does not take over (see th06/settings.h), and ignore
// changes to them. The original measures the frame rate on the title to
// slow the game down on fast displays, and waits until three seconds after
// the start before it plays the title music. The port does neither.

// Adds the title at calc priority 2 and draw priority 0. Opens the replay
// menu when mode is nonzero, or the title otherwise. The first calc loads
// the files of the screen it opens.
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
