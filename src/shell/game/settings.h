#pragma once

#include "core/config.h"

#include <stdint.h>

// Scales the screen to the window: square pixels as large as the window
// allows, a smooth filter, or square pixels at a whole multiple.
typedef enum {
    SHELL_SCALING_SHARP,
    SHELL_SCALING_SMOOTH,
    SHELL_SCALING_INTEGER,
} SHELL_SCALING;

// Holds the shell's own settings, loaded and saved by shell/game.
typedef struct {
    bool fullscreen;
    bool interpolation; // smooths motion between ticks
    int32_t scaling; // a SHELL_SCALING
    int32_t screenshot_resolution;
    int32_t screenshot_format;
    int32_t screenshot_jpeg_quality;
} SHELL_SETTINGS;

extern const CONFIG_SCHEMA g_ShellSettingsSchema;
