#pragma once

#include "core/base/result.h"
#include "shell/game/settings.h"

// Saves the last rendered frame to the user's screenshots, named and
// scaled per the settings the shell's hotkey triggers this with.

RESULT Screenshot_Save(
    const char *user_dir, const char *game_id, const SHELL_SETTINGS *settings);
