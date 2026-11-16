#pragma once

#include "core/base/result.h"

// Runs the ending and the staff roll: the script of th07/screens/ending/
// script.h shows text lines, pictures and backgrounds, changes the music,
// scrolls and fades. When the script ends, the ending removes itself and
// requests SCENE_GAME_RESULT. A script that runs past its last byte ends
// there, where the original reads on.
//
// Since a clear of the same shot and difficulty was recorded before, or
// after a script loaded with "@F" finds any clear of difficulty 0 to 3 by
// any shot, the skip button runs the script and the sprites up to five
// times a calc. The fade steps once a draw.

// Adds the ending at calc priority 4 and draw priority 1. Records the clear
// of the shot and difficulty and loads the staff sprites and the script:
// the bad ending of the character after a continue, else the good ending
// of the shot. Reports failure when a file fails to load.
RESULT Ending_Register(void);
