#include "shell/fatal.h"

#include "core/base/log.h"
#include "shell/log_file.h"

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
    Fatal_FormatInternalError(
        text, sizeof(text), m_GameId, message, file, line, LogFile_GetPath());
    M_Show("stopped", text);
}

bool Fatal_UsesBox(const char *const video_driver, const char *const ci)
{
    if (ci != nullptr && ci[0] != '\0') {
        return false;
    }
    return video_driver == nullptr
        || (SDL_strcasecmp(video_driver, "offscreen") != 0
            && SDL_strcasecmp(video_driver, "dummy") != 0);
}

void Fatal_Install(const char *const game_id)
{
    m_GameId = game_id;
    m_UsesBox = Fatal_UsesBox(SDL_getenv("SDL_VIDEODRIVER"), SDL_getenv("CI"));
    if (m_UsesBox) {
        Log_SetFatalHandler(M_HandleFatal);
    }
}

void Fatal_ShowStartupError(const char *const text)
{
    if (m_UsesBox) {
        M_Show("could not start", text);
    }
}

void Fatal_FormatMissingData(
    char *const out, const size_t size, const char *const file,
    const char *const data_dir, const char *const program_name,
    const char *const log_path)
{
    // macOS starts a downloaded bundle from a random read-only copy under
    // this folder until its quarantine attribute is removed.
    if (strstr(data_dir, "/AppTranslocation/") != nullptr) {
        snprintf(
            out, size,
            "Cannot find the game data file %s.\n"
            "\n"
            "Searched folder: %s\n"
            "\n"
            "macOS started a temporary copy of the program, because the "
            "program came from the internet. In Terminal, go to the folder "
            "of the original game and type:\n"
            "\n"
            "xattr -dr com.apple.quarantine %s.app\n"
            "\n"
            "Then start the program again.",
            file, data_dir, program_name);
    } else {
        snprintf(
            out, size,
            "Cannot find the game data file %s.\n"
            "\n"
            "Searched folder: %s\n"
            "\n"
            "Put the program into the folder of the original game, or "
            "start it with --data <folder>.",
            file, data_dir);
    }
    M_AddLogPath(out, size, log_path);
}

void Fatal_FormatStartupError(
    char *const out, const size_t size, const char *const message,
    const char *const log_path)
{
    snprintf(out, size, "%s", message);
    M_AddLogPath(out, size, log_path);
}

void Fatal_FormatInternalError(
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
