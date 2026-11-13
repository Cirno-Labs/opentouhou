#include "settings.h"

static const char *const m_Scalings[] = { "sharp", "smooth", "integer",
                                          nullptr };
static const char *const m_ScreenshotResolutions[] = { "native", "window",
                                                       nullptr };
static const char *const m_ScreenshotFormats[] = { "png", "jpg", nullptr };

static const CONFIG_OPTION m_Options[] = {
    CONFIG_BOOL(SHELL_SETTINGS, "video", "fullscreen", fullscreen, false),
    CONFIG_BOOL(SHELL_SETTINGS, "video", "interpolation", interpolation, true),
    CONFIG_ENUM(
        SHELL_SETTINGS, "video", "scaling", scaling, SHELL_SCALING_SHARP,
        m_Scalings),
    CONFIG_ENUM(
        SHELL_SETTINGS, "screenshot", "resolution", screenshot_resolution, 0,
        m_ScreenshotResolutions),
    CONFIG_ENUM(
        SHELL_SETTINGS, "screenshot", "format", screenshot_format, 0,
        m_ScreenshotFormats),
    CONFIG_INT(
        SHELL_SETTINGS, "screenshot", "jpeg_quality", screenshot_jpeg_quality,
        90, 1, 100),
};

const CONFIG_SCHEMA g_ShellSettingsSchema =
    CONFIG_SCHEMA_OF(SHELL_SETTINGS, m_Options);
