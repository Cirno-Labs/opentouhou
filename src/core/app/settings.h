#pragma once

#include "core/config.h"

#include <stdint.h>

// Holds core's own settings, loaded and saved by core/app.
typedef struct {
    bool fullscreen;
    bool interpolation; // smooths motion between ticks
    int32_t screenshot_resolution;
    int32_t screenshot_format;
    int32_t screenshot_jpeg_quality;
    bool patches_enabled;
    // Lists the ids of the patches that are left out, separated by commas.
    char patches_disabled[CONFIG_STRING_SIZE];
} APP_SETTINGS;

extern const CONFIG_SCHEMA g_AppSettingsSchema;
