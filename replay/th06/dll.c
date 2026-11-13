#include "../core/clock.h"
#include "../core/overlay.h"
#include "../core/patch.h"
#include "fields.h"
#include "retail.h"
#include "state.h"

#include "core/base/log.h"
#include "core/base/result.h"
#include "core/base/utils.h"
#include "core/io/writer.h"
#include "../core/case.h"

#include <windows.h>

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// Starts the replay or opens the result screen after this many frames on
// the title screen.
#define TITLE_DELAY 60

// More than one 60 Hz frame, so every RunFrame runs one calc without
// waiting for real time.
#define FAST_CLOCK_STEP 17

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
typedef int(__attribute__((thiscall)) * RUN_CALC_CHAIN_FN)(void *chain);
#pragma GCC diagnostic pop

static char m_ReplayPath[MAX_PATH];
static bool m_ReplayStarted;
static bool m_ReplayEnded;
static bool m_ResultOpened;
static int m_TitleFrames;
static uint32_t m_Calcs;
static DWORD m_QuitAfter;

static WRITER m_CaseFile;
static CASE_WRITER *m_Case;
static uint32_t m_CaseRows;

static bool M_GetEnv(const wchar_t *name, wchar_t *out, DWORD size)
{
    const DWORD length = GetEnvironmentVariableW(name, out, size);
    return length != 0 && length < size;
}

static void M_OpenCase(void)
{
    wchar_t path_w[MAX_PATH];
    if (!M_GetEnv(L"TH06CAPTURE_CASE", path_w, MAX_PATH)) {
        return;
    }
    char path[MAX_PATH];
    WideCharToMultiByte(
        CP_ACP, 0, path_w, -1, path, sizeof(path), nullptr, nullptr);
    if (!Result_Log(Writer_OpenFile(&m_CaseFile, path), LOG_LEVEL_ERROR)) {
        return;
    }

    const char *start_names[] = { TH06_START_NAMES };
    uint32_t start_row[TH06_START_COUNT];
    State_Start(start_row);
    const char *calc_names[] = { TH06_CALC_NAMES };
    const CASE_HEADER header = {
        .game_id = "th06",
        .info = "th06 1.02h",
        .start_names = { .items = start_names,
                         .count = ARRAY_SIZE(start_names) },
        .start = { .items = start_row, .count = ARRAY_SIZE(start_row) },
        .calc_names = { .items = calc_names, .count = ARRAY_SIZE(calc_names) },
    };
    m_Case = Case_OpenWriter(&m_CaseFile, &header);
    m_CaseRows = 0;
    LOG_INFO("case: opened %s", path);
}

static void M_WriteCaseRow(void)
{
    uint32_t row[TH06_CALC_COUNT];
    State_Calc(row);
    Case_WriteRow(m_Case, row);
    m_CaseRows++;
}

static void M_EndOfCalc(void)
{
    m_Calcs++;
    if (m_Calcs % 600 == 0) {
        LOG_INFO("calcs: %u", m_Calcs);
    }
    if (m_Case == nullptr && RETAIL_GAME_MANAGER_AT->is_playing != 0
        && RETAIL_GAME_MANAGER_AT->is_in_demo == 0) {
        M_OpenCase();
    }
    if (m_Case != nullptr) {
        M_WriteCaseRow();
    }
}

static void M_CloseCase(void)
{
    if (m_Case == nullptr) {
        return;
    }
    Result_Log(Case_CloseWriter(m_Case), LOG_LEVEL_ERROR);
    Result_Log(Writer_Close(&m_CaseFile), LOG_LEVEL_ERROR);
    LOG_INFO("case: closed, %u rows", m_CaseRows);
    m_Case = nullptr;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
static int __attribute__((thiscall)) M_RunCalcChain(void *chain)
{
    const int result = ((RUN_CALC_CHAIN_FN)RETAIL_RUN_CALC_CHAIN)(chain);
    M_EndOfCalc();
    return result;
}
#pragma GCC diagnostic pop

static int __cdecl M_PlayOnUpdate2(void *replay)
{
    const int result = ((RETAIL_CALLBACK)RETAIL_PLAY_ON_UPDATE_2)(replay);
    if (result == RETAIL_CHAIN_RESTART) {
        M_EndOfCalc();
    }
    return result;
}

// Does what confirming a file in the replay menu does, for the replay's
// first stage.
static void M_StartReplay(void)
{
    RETAIL_REPLAY_HEADER *const header = RETAIL_LOAD_FILE_DATA(m_ReplayPath, 1);
    if (header == nullptr
        || RETAIL_VALIDATE_REPLAY(header, RETAIL_LAST_FILE_SIZE) != 0) {
        LOG_FATAL("cannot load %s", m_ReplayPath);
    }
    int stage = 1;
    while (stage < 8 && header->stages[stage] == 0) {
        stage++;
    }
    if (stage == 8) {
        LOG_FATAL("%s has no stage", m_ReplayPath);
    }
    const RETAIL_REPLAY_STAGE *const first =
        (RETAIL_REPLAY_STAGE *)((uint8_t *)header + header->stages[stage]);

    RETAIL_GAME_MANAGER *const game = RETAIL_GAME_MANAGER_AT;
    game->is_in_replay = 1;
    strncpy(game->replay_file, m_ReplayPath, sizeof(game->replay_file) - 1);
    game->difficulty = header->difficulty;
    game->character = header->shot_type / 2;
    game->shot_type = header->shot_type % 2;
    game->lives = (int8_t)first->lives;
    game->bombs = (int8_t)first->bombs;
    game->stage = stage - 1;
    RETAIL_APP_AT->scene = RETAIL_SCENE_GAMEPLAY;
    RETAIL_FREE(header);
    LOG_INFO("replay: started at stage %d", stage);
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

static bool M_TitleDelayReached(const void *const menu)
{
    const int state = ((const RETAIL_MAIN_MENU *)menu)->state;
    const bool title =
        state == RETAIL_MAIN_MENU_TITLE || state == RETAIL_MAIN_MENU_TITLE_KEY;
    m_TitleFrames = title ? m_TitleFrames + 1 : 0;
    return m_TitleFrames == TITLE_DELAY;
}

static int __cdecl M_MainMenuOnUpdate(void *menu)
{
    if (m_ReplayStarted) {
        if (!m_ReplayEnded) {
            m_ReplayEnded = true;
            EnumWindows(M_CloseGameWindow, 0);
            LOG_INFO("replay: ended, closing the game");
        }
        return ((RETAIL_CALLBACK)RETAIL_MAIN_MENU_ON_UPDATE)(menu);
    }
    if (M_TitleDelayReached(menu)) {
        M_StartReplay();
        m_ReplayStarted = true;
        return 0;
    }
    return ((RETAIL_CALLBACK)RETAIL_MAIN_MENU_ON_UPDATE)(menu);
}

// Does what choosing Result on the title screen does. The result screen
// loads score.dat and saves it when it closes, also when the game closes.
static int __cdecl M_MainMenuOpenResult(void *menu)
{
    if (!m_ResultOpened && M_TitleDelayReached(menu)) {
        m_ResultOpened = true;
        RETAIL_APP_AT->scene = RETAIL_SCENE_RESULT;
        LOG_INFO("result: opened");
        return 0;
    }
    return ((RETAIL_CALLBACK)RETAIL_MAIN_MENU_ON_UPDATE)(menu);
}

static DWORD WINAPI M_QuitAfterThread(LPVOID unused)
{
    Sleep(m_QuitAfter * 1000);
    EnumWindows(M_CloseGameWindow, 0);
    return 0;
}

static bool M_IsUnder(const wchar_t *dir, const wchar_t *path)
{
    const size_t length = wcslen(dir);
    return _wcsnicmp(dir, path, length) == 0
        && (path[length] == L'\0' || path[length] == L'\\');
}

static void M_Patch(RESULT result)
{
    if (!Result_Log(result, LOG_LEVEL_ERROR)) {
        TerminateProcess(GetCurrentProcess(), 1);
    }
}

static void M_Attach(void)
{
    // Without a safe work directory the game would write into its own.
    wchar_t work[MAX_PATH];
    wchar_t game[MAX_PATH];
    if (!M_GetEnv(L"TH06CAPTURE_WORK", work, MAX_PATH) || work[1] != L':'
        || GetCurrentDirectoryW(MAX_PATH, game) == 0 || M_IsUnder(game, work)) {
        TerminateProcess(GetCurrentProcess(), 1);
    }
    M_Patch(Overlay_Install(game, work));
    wchar_t flag[4];
    const uint32_t step =
        M_GetEnv(L"TH06CAPTURE_FAST", flag, 4) ? FAST_CLOCK_STEP : 0;
    if (!Clock_Install(GetModuleHandleW(nullptr), step)) {
        LOG_ERROR("timeGetTime is not imported");
        TerminateProcess(GetCurrentProcess(), 1);
    }
    M_Patch(Patch_Call(
        RETAIL_RUN_CALC_CHAIN_CALL, RETAIL_RUN_CALC_CHAIN,
        (uintptr_t)M_RunCalcChain));

    wchar_t replay[MAX_PATH];
    if (M_GetEnv(L"TH06CAPTURE_REPLAY", replay, MAX_PATH)) {
        WideCharToMultiByte(
            CP_ACP, 0, replay, -1, m_ReplayPath, sizeof(m_ReplayPath), nullptr,
            nullptr);
        M_Patch(Patch_Value32(
            RETAIL_TITLE_CALLBACK_SITE, RETAIL_MAIN_MENU_ON_UPDATE,
            (uint32_t)(uintptr_t)M_MainMenuOnUpdate));
        M_Patch(Patch_Value32(
            RETAIL_PLAY_ON_UPDATE_2_SITE, RETAIL_PLAY_ON_UPDATE_2,
            (uint32_t)(uintptr_t)M_PlayOnUpdate2));
    } else if (M_GetEnv(L"TH06CAPTURE_RESULT", flag, 4)) {
        M_Patch(Patch_Value32(
            RETAIL_TITLE_CALLBACK_SITE, RETAIL_MAIN_MENU_ON_UPDATE,
            (uint32_t)(uintptr_t)M_MainMenuOpenResult));
    }

    wchar_t quit_after[16];
    if (M_GetEnv(L"TH06CAPTURE_QUIT_AFTER", quit_after, 16)) {
        m_QuitAfter = (DWORD)_wtoi(quit_after);
        CreateThread(nullptr, 0, M_QuitAfterThread, nullptr, 0, nullptr);
    }
    LOG_INFO("dll: ready");
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        M_Attach();
    } else if (reason == DLL_PROCESS_DETACH) {
        M_CloseCase();
    }
    return TRUE;
}
