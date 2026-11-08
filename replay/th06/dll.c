#include "../core/clock.h"
#include "../core/overlay.h"
#include "../core/patch.h"
#include "retail.h"

#include "core/base/log.h"
#include "core/base/result.h"

#include <windows.h>

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// Starts the replay after this many frames on the title screen.
#define REPLAY_START_DELAY 60

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
typedef int(__attribute__((thiscall)) * RUN_CALC_CHAIN_FN)(void *chain);
#pragma GCC diagnostic pop

static char m_ReplayPath[MAX_PATH];
static int m_TitleFrames;
static uint32_t m_Calcs;
static DWORD m_QuitAfter;

static bool M_GetEnv(const wchar_t *name, wchar_t *out, DWORD size)
{
    const DWORD length = GetEnvironmentVariableW(name, out, size);
    return length != 0 && length < size;
}

static void M_EndOfCalc(void)
{
    m_Calcs++;
    if (m_Calcs % 600 == 0) {
        LOG_INFO("calcs: %u", m_Calcs);
    }
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

static int __cdecl M_MainMenuOnUpdate(void *menu)
{
    const int state = ((RETAIL_MAIN_MENU *)menu)->state;
    const bool title =
        state == RETAIL_MAIN_MENU_TITLE || state == RETAIL_MAIN_MENU_TITLE_KEY;
    m_TitleFrames = title ? m_TitleFrames + 1 : 0;
    if (m_TitleFrames == REPLAY_START_DELAY) {
        M_StartReplay();
        return 0;
    }
    return ((RETAIL_CALLBACK)RETAIL_MAIN_MENU_ON_UPDATE)(menu);
}

static BOOL CALLBACK M_CloseGameWindow(HWND window, LPARAM unused)
{
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process != GetCurrentProcessId() || !IsWindowVisible(window)) {
        return TRUE;
    }
    PostMessageW(window, WM_CLOSE, 0, 0);
    LOG_INFO("quit-after: closed the game window");
    return FALSE;
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
    if (!Clock_Install(GetModuleHandleW(nullptr))) {
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
    }
    return TRUE;
}
