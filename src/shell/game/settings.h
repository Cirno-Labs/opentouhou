#pragma once

#include "core/config.h"

#include <stdint.h>

// Holds the shell's own settings, loaded and saved by shell/game.
typedef struct {
    bool fullscreen;
    bool interpolation; // smooths motion between ticks
    int32_t screenshot_resolution;
    int32_t screenshot_format;
    int32_t screenshot_jpeg_quality;
} APP_SETTINGS;

extern const CONFIG_SCHEMA g_AppSettingsSchema;
