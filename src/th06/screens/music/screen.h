#pragma once

#include "core/base/result.h"

// Runs the music room: the list of tunes, ten at a time, with the comment
// of the tune that plays. Confirm plays the tune at the cursor through
// th06/sound, which plays nothing while the music setting is off. Cancel
// returns to the title and leaves the tune playing.

// Adds the music room at calc priority 2 and draw priority 0, and loads
// its images and data/musiccmt.txt. Reports failure when a file fails to
// load.
RESULT MusicScreen_Register(void);
