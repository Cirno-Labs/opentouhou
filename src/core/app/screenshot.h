// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "core/app/settings.h"
#include "core/base/result.h"

RESULT Screenshot_Save(
    const char *user_dir, const char *game_id, const APP_SETTINGS *settings);
