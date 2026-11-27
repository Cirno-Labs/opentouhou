#pragma once

#include "core/base/result.h"
#include "shell/game/settings.h"

RESULT Screenshot_Save(
    const char *user_dir, const char *game_id, const SHELL_SETTINGS *settings);
