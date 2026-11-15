#include "capture.h"

#include "case.h"
#include "clock.h"
#include "null_d3d8.h"
#include "overlay.h"

#include "core/base/log.h"
#include "core/base/memory.h"
#include "core/base/result.h"
#include "core/io/writer.h"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

// Starts the replay or opens the result screen after this many frames on
// the title screen.
#define TITLE_DELAY 60

// More than one 60 Hz frame, so every RunFrame runs one calc without
// waiting for real time.
#define FAST_CLOCK_STEP 17

static const CAPTURE_CASE *m_Capture;
static int m_TitleFrames;
static uint32_t m_Calcs;
static DWORD m_QuitAfter;

static WRITER m_CaseFile;
static CASE_WRITER *m_Case;
static uint32_t *m_Row;
static uint32_t m_CaseRows;
static bool m_CaseDone;

static bool M_GetEnv(const wchar_t *name, wchar_t *out, DWORD size)
{
    const DWORD length = GetEnvironmentVariableW(name, out, size);
    return length != 0 && length < size;
}

static bool M_IsUnder(const wchar_t *dir, const wchar_t *path)
{
    const size_t length = wcslen(dir);
    return _wcsnicmp(dir, path, length) == 0
        && (path[length] == L'\0' || path[length] == L'\\');
}

static BOOL CALLBACK M_CloseGameWindow(HWND window, LPARAM unused)
{
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process != GetCurrentProcessId() || !IsWindowVisible(window)) {
        return TRUE;
    }
    PostMessageW(window, WM_CLOSE, 0, 0);
    LOG_INFO("closed the game window");
    return FALSE;
}

static DWORD WINAPI M_QuitAfterThread(LPVOID unused)
{
    Sleep(m_QuitAfter * 1000);
    Capture_CloseGame();
    return 0;
}

CAPTURE_MODE Capture_Attach(
    const CAPTURE_CASE *const capture, char *const replay, const size_t size)
{
    m_Capture = capture;
    m_Row = Mem_AllocArr(capture->calc_count, sizeof(uint32_t));

    // Without a safe work directory the game would write into its own.
    wchar_t work[MAX_PATH];
    wchar_t game[MAX_PATH];
    if (!M_GetEnv(L"CAPTURE_WORK", work, MAX_PATH) || work[1] != L':'
        || GetCurrentDirectoryW(MAX_PATH, game) == 0 || M_IsUnder(game, work)) {
        TerminateProcess(GetCurrentProcess(), 1);
    }
    Capture_Check(Overlay_Install(game, work));
    wchar_t flag[4];
    const bool fast = M_GetEnv(L"CAPTURE_FAST", flag, 4);
    if (!Clock_Install(GetModuleHandleW(nullptr), fast ? FAST_CLOCK_STEP : 0)) {
        LOG_ERROR("no clock function is imported");
        TerminateProcess(GetCurrentProcess(), 1);
    }
    if (fast && !M_GetEnv(L"CAPTURE_DRAW", flag, 4)
        && !NullD3d8_Install(GetModuleHandleW(nullptr))) {
        LOG_ERROR("Direct3DCreate8 is not imported");
        TerminateProcess(GetCurrentProcess(), 1);
    }

    wchar_t quit_after[16];
    if (M_GetEnv(L"CAPTURE_QUIT_AFTER", quit_after, 16)) {
        m_QuitAfter = (DWORD)_wtoi(quit_after);
        CreateThread(nullptr, 0, M_QuitAfterThread, nullptr, 0, nullptr);
    }

    wchar_t replay_w[MAX_PATH];
    if (M_GetEnv(L"CAPTURE_REPLAY", replay_w, MAX_PATH)) {
        WideCharToMultiByte(
            CP_ACP, 0, replay_w, -1, replay, (int)size, nullptr, nullptr);
        return CAPTURE_REPLAY;
    }
    return M_GetEnv(L"CAPTURE_RESULT", flag, 4) ? CAPTURE_RESULT : CAPTURE_LIVE;
}

void Capture_Check(const RESULT result)
{
    if (!Result_Log(result, LOG_LEVEL_ERROR)) {
        TerminateProcess(GetCurrentProcess(), 1);
    }
}

void Capture_CountCalc(void)
{
    m_Calcs++;
    if (m_Calcs % 600 == 0) {
        LOG_INFO("calcs: %u", m_Calcs);
    }
}

void Capture_OpenCase(void)
{
    if (m_Case != nullptr || m_CaseDone) {
        return;
    }
    m_CaseDone = true;
    wchar_t path_w[MAX_PATH];
    if (!M_GetEnv(L"CAPTURE_CASE", path_w, MAX_PATH)) {
        return;
    }
    char path[MAX_PATH];
    WideCharToMultiByte(
        CP_ACP, 0, path_w, -1, path, sizeof(path), nullptr, nullptr);
    if (!Result_Log(Writer_OpenFile(&m_CaseFile, path), LOG_LEVEL_ERROR)) {
        return;
    }

    uint32_t *const start_row =
        Mem_AllocArr(m_Capture->start_count, sizeof(uint32_t));
    m_Capture->start(start_row);
    CASE_HEADER header = {
        .info = m_Capture->info,
        .start_names = { .items = (const char **)m_Capture->start_names,
                         .count = (int32_t)m_Capture->start_count },
        .start = { .items = start_row,
                   .count = (int32_t)m_Capture->start_count },
        .calc_names = { .items = (const char **)m_Capture->calc_names,
                        .count = (int32_t)m_Capture->calc_count },
    };
    snprintf(header.game_id, sizeof(header.game_id), "%s", m_Capture->game_id);
    m_Case = Case_OpenWriter(&m_CaseFile, &header);
    Mem_Free(start_row);
    m_CaseRows = 0;
    LOG_INFO("case: opened %s", path);
}

void Capture_CloseCase(void)
{
    if (m_Case == nullptr) {
        return;
    }
    Result_Log(Case_CloseWriter(m_Case), LOG_LEVEL_ERROR);
    Result_Log(Writer_Close(&m_CaseFile), LOG_LEVEL_ERROR);
    if (m_Capture->closed != nullptr) {
        m_Capture->closed(m_CaseRows);
    } else {
        LOG_INFO("case: closed, %u rows", m_CaseRows);
    }
    m_Case = nullptr;
}

void Capture_WriteRow(void)
{
    if (m_Case == nullptr) {
        return;
    }
    m_Capture->calc(m_Row);
    Case_WriteRow(m_Case, m_Row);
    m_CaseRows++;
}

bool Capture_TitleDelayReached(const bool on_title)
{
    m_TitleFrames = on_title ? m_TitleFrames + 1 : 0;
    return m_TitleFrames == TITLE_DELAY;
}

void Capture_CloseGame(void)
{
    EnumWindows(M_CloseGameWindow, 0);
}
