#include "settings.h"

static const char *const m_ScreenshotResolutions[] = { "native", "window",
                                                       nullptr };
static const char *const m_ScreenshotFormats[] = { "png", "jpg", nullptr };

static const CONFIG_OPTION m_Options[] = {
    CONFIG_BOOL(APP_SETTINGS, "video", "fullscreen", fullscreen, false),
    CONFIG_BOOL(APP_SETTINGS, "video", "interpolation", interpolation, true),
    CONFIG_ENUM(
        APP_SETTINGS, "screenshot", "resolution", screenshot_resolution, 0,
        m_ScreenshotResolutions),
    CONFIG_ENUM(
        APP_SETTINGS, "screenshot", "format", screenshot_format, 0,
        m_ScreenshotFormats),
    CONFIG_INT(
        APP_SETTINGS, "screenshot", "jpeg_quality", screenshot_jpeg_quality, 90,
        1, 100),
    CONFIG_BOOL(APP_SETTINGS, "patches", "enabled", patches_enabled, true),
    CONFIG_STRING(APP_SETTINGS, "patches", "disabled", patches_disabled, ""),
};

const CONFIG_SCHEMA g_AppSettingsSchema =
    CONFIG_SCHEMA_OF(APP_SETTINGS, m_Options);
