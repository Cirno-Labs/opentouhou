#pragma once

#include "shell/game/settings.h"
#include "core/base/result.h"

RESULT Screenshot_Save(
    const char *user_dir, const char *game_id, const SHELL_SETTINGS *settings);
