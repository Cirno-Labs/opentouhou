#pragma once

#include "core/config.h"

#include <stdint.h>

// Holds core's own settings, loaded and saved by core/app.
typedef struct {
    bool fullscreen;
    int32_t screenshot_resolution;
    int32_t screenshot_format;
    int32_t screenshot_jpeg_quality;
} APP_SETTINGS;

extern const CONFIG_SCHEMA g_AppSettingsSchema;
