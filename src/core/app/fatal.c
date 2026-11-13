// SPDX-License-Identifier: GPL-3.0-only

#include "fatal.h"

#include "log_file.h"

#include "core/base/log.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

#define M_TEXT_SIZE 4096
#define M_TITLE_SIZE 128

static bool m_UsesBox = false;
static const char *m_GameId = "";

static void M_Show(const char *const outcome, const char *const text)
{
    char title[M_TITLE_SIZE];
    snprintf(title, sizeof(title), "%s %s", m_GameId, outcome);
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, text, nullptr)) {
        LOG_WARNING("Cannot show a message box: %s", SDL_GetError());
    }
}

static void M_AddLogPath(
    char *const out, const size_t size, const char *const log_path)
{
    const size_t length = strlen(out);
    if (log_path != nullptr && length + 1 < size) {
        snprintf(out + length, size - length, "\n\nLog file: %s", log_path);
    }
}

static void M_HandleFatal(
    const char *const message, const char *const file, const int line)
{
    char text[M_TEXT_SIZE];
    AppFatal_FormatInternalError(
        text, sizeof(text), m_GameId, message, file, line,
        AppLogFile_GetPath());
    M_Show("stopped", text);
}

bool AppFatal_UsesBox(const char *const video_driver, const char *const ci)
{
    if (ci != nullptr && ci[0] != '\0') {
        return false;
    }
    return video_driver == nullptr
        || (SDL_strcasecmp(video_driver, "offscreen") != 0
            && SDL_strcasecmp(video_driver, "dummy") != 0);
}

void AppFatal_Install(const char *const game_id)
{
    m_GameId = game_id;
    m_UsesBox =
        AppFatal_UsesBox(SDL_getenv("SDL_VIDEODRIVER"), SDL_getenv("CI"));
    if (m_UsesBox) {
        Log_SetFatalHandler(M_HandleFatal);
    }
}

void AppFatal_ShowStartupError(const char *const text)
{
    if (m_UsesBox) {
        M_Show("could not start", text);
    }
}

void AppFatal_FormatMissingData(
    char *const out, const size_t size, const char *const file,
    const char *const data_dir, const char *const log_path)
{
    snprintf(
        out, size,
        "Cannot find the game data file %s.\n"
        "\n"
        "Searched folder: %s\n"
        "\n"
        "Put the program into the folder of the original game, or start "
        "it with --data <folder>.",
        file, data_dir);
    M_AddLogPath(out, size, log_path);
}

void AppFatal_FormatStartupError(
    char *const out, const size_t size, const char *const message,
    const char *const log_path)
{
    snprintf(out, size, "%s", message);
    M_AddLogPath(out, size, log_path);
}

void AppFatal_FormatInternalError(
    char *const out, const size_t size, const char *const game_id,
    const char *const message, const char *const file, const int line,
    const char *const log_path)
{
    snprintf(
        out, size,
        "%s stopped because of an internal error:\n"
        "\n"
        "%s (%s:%d)",
        game_id, message, file, line);
    M_AddLogPath(out, size, log_path);
}
