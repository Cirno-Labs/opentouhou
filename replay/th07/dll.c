#include "../core/case.h"
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

#include <windows.h>

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// Starts the replay or opens the result screen after this many updates on
// the title screen.
#define TITLE_DELAY 60

// More than one 60 Hz frame, so every RunFrame runs one calc without
// waiting for real time.
#define FAST_CLOCK_STEP 17

#define CALL_OPCODE 0xe8
#define CALL_SIZE 5

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
static uint32_t m_FastForwards;
static DWORD m_QuitAfter;

static WRITER m_CaseFile;
static CASE_WRITER *m_Case;
static uint32_t m_CaseRows;
static bool m_CaseDone;

static bool M_GetEnv(const wchar_t *name, wchar_t *out, DWORD size)
{
    const DWORD length = GetEnvironmentVariableW(name, out, size);
    return length != 0 && length < size;
}

static void M_OpenCase(void)
{
    wchar_t path_w[MAX_PATH];
    if (!M_GetEnv(L"TH07CAPTURE_CASE", path_w, MAX_PATH)) {
        m_CaseDone = true;
        return;
    }
    char path[MAX_PATH];
    WideCharToMultiByte(
        CP_ACP, 0, path_w, -1, path, sizeof(path), nullptr, nullptr);
    if (!Result_Log(Writer_OpenFile(&m_CaseFile, path), LOG_LEVEL_ERROR)) {
        m_CaseDone = true;
        return;
    }

    const char *start_names[] = { TH07_START_NAMES };
    uint32_t start_row[TH07_START_COUNT];
    State_Start(start_row);
    const char *calc_names[] = { TH07_CALC_NAMES };
    const CASE_HEADER header = {
        .game_id = "th07",
        .info = "th07 1.00b",
        .start_names = { .items = start_names,
                         .count = ARRAY_SIZE(start_names) },
        .start = { .items = start_row, .count = ARRAY_SIZE(start_row) },
        .calc_names = { .items = calc_names, .count = ARRAY_SIZE(calc_names) },
    };
    m_Case = Case_OpenWriter(&m_CaseFile, &header);
    m_CaseRows = 0;
    LOG_INFO("case: opened %s", path);
}

static void M_CloseCase(void)
{
    if (m_Case == nullptr) {
        return;
    }
    Result_Log(Case_CloseWriter(m_Case), LOG_LEVEL_ERROR);
    Result_Log(Writer_Close(&m_CaseFile), LOG_LEVEL_ERROR);
    LOG_INFO(
        "case: closed, %u rows, %u fast-forwards", m_CaseRows, m_FastForwards);
    m_Case = nullptr;
    m_CaseDone = true;
}

static void M_WriteCaseRow(void)
{
    uint32_t row[TH07_CALC_COUNT];
    State_Calc(row);
    Case_WriteRow(m_Case, row);
    m_CaseRows++;
}

// Writes rows while the game plays, from the first calc of play (not the
// title's demo) to the last one before it leaves the gameplay scene.
// The title scene gives GameManager new, uninitialized stats.
static void M_EndOfCalc(void)
{
    m_Calcs++;
    if (m_Calcs % 600 == 0) {
        LOG_INFO("calcs: %u", m_Calcs);
    }
    const RETAIL_GAME_MANAGER *const game = RETAIL_GAME_MANAGER_AT;
    const bool gameplay = RETAIL_APP_AT->cur_scene == RETAIL_SCENE_GAMEPLAY;
    if (m_Case != nullptr && !gameplay) {
        M_CloseCase();
    }
    if (m_Case == nullptr && !m_CaseDone && gameplay
        && (game->state_flags & RETAIL_STATE_PLAYING) != 0
        && (game->state_flags & RETAIL_STATE_DEMO) == 0) {
        M_OpenCase();
    }
    if (m_Case != nullptr) {
        M_WriteCaseRow();
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

// Dialogue fast-forward ends the calc here and starts the next one.
static int __attribute__((fastcall)) M_PlayOnUpdate2(void *replay)
{
    const int result = ((RETAIL_CALLBACK)RETAIL_PLAY_ON_UPDATE_2)(replay);
    if (result == RETAIL_CHAIN_RESTART) {
        m_FastForwards++;
        M_EndOfCalc();
    }
    return result;
}
#pragma GCC diagnostic pop

// Does what confirming a file in the replay menu does, with normal
// playback, for the replay's first stage. Phantasm is stage 8, in the
// last slot.
static void M_StartReplay(void)
{
    void *const file = RETAIL_LOAD_FILE_DATA(m_ReplayPath, 1);
    RETAIL_REPLAY_HEADER *const header = file == nullptr
        ? nullptr
        : RETAIL_VALIDATE_REPLAY(file, RETAIL_LAST_FILE_SIZE);
    if (header == nullptr) {
        LOG_FATAL("cannot load %s", m_ReplayPath);
    }
    int slot = 0;
    while (slot < 7 && header->stages[slot] == 0) {
        slot++;
    }
    if (slot == 7) {
        LOG_FATAL("%s has no stage", m_ReplayPath);
    }

    RETAIL_GAME_MANAGER *const game = RETAIL_GAME_MANAGER_AT;
    game->state_flags |= RETAIL_STATE_IN_REPLAY;
    strncpy(game->replay_file, m_ReplayPath, sizeof(game->replay_file) - 1);
    game->difficulty = header->difficulty;
    game->character = header->shot_type / 2;
    game->shot_type = header->shot_type % 2;
    game->shot_index = header->shot_type;
    game->stage = header->difficulty >= RETAIL_DIFFICULTY_PHANTASM
        ? RETAIL_PHANTASM_START_STAGE
        : slot;
    RETAIL_APP_AT->scene = RETAIL_SCENE_GAMEPLAY;
    game->replay_menu_choice = 0;
    RETAIL_FREE(header);
    LOG_INFO("replay: started at stage %d", game->stage + 1);
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
    const RETAIL_MAIN_MENU *const main_menu = menu;
    const bool title = main_menu->state == RETAIL_MAIN_MENU_TITLE
        && main_menu->sub_state == RETAIL_MAIN_MENU_TITLE_SUB_STATE;
    m_TitleFrames = title ? m_TitleFrames + 1 : 0;
    return m_TitleFrames == TITLE_DELAY;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
static int __attribute__((fastcall)) M_MainMenuOnUpdate(void *menu)
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
// saves score.dat when it closes, also when the game closes.
static int __attribute__((fastcall)) M_MainMenuOpenResult(void *menu)
{
    if (!m_ResultOpened && M_TitleDelayReached(menu)) {
        m_ResultOpened = true;
        RETAIL_APP_AT->scene = RETAIL_SCENE_RESULT;
        LOG_INFO("result: opened");
        return 0;
    }
    return ((RETAIL_CALLBACK)RETAIL_MAIN_MENU_ON_UPDATE)(menu);
}
#pragma GCC diagnostic pop

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

// Finds the one call to target in the size bytes of code at start.
static RESULT M_FindCall(
    uintptr_t start, size_t size, uintptr_t target, uintptr_t *out_site)
{
    size_t count = 0;
    for (uintptr_t site = start; site + CALL_SIZE <= start + size; site++) {
        int32_t offset;
        memcpy(&offset, (const void *)(site + 1), sizeof(offset));
        if (*(const uint8_t *)site == CALL_OPCODE
            && site + CALL_SIZE + (uintptr_t)offset == target) {
            *out_site = site;
            count++;
        }
    }
    FAIL_IF(
        count != 1, "0x%08x: %zu calls to 0x%08x", (uint32_t)start, count,
        (uint32_t)target);
    return OK;
}

// Finds the one 32-bit value in the size bytes of code at start.
static RESULT M_FindValue32(
    uintptr_t start, size_t size, uint32_t value, uintptr_t *out_site)
{
    size_t count = 0;
    for (uintptr_t site = start; site + sizeof(value) <= start + size; site++) {
        uint32_t found;
        memcpy(&found, (const void *)site, sizeof(found));
        if (found == value) {
            *out_site = site;
            count++;
        }
    }
    FAIL_IF(
        count != 1, "0x%08x: %zu copies of 0x%08x", (uint32_t)start, count,
        value);
    return OK;
}

static RESULT M_PatchCallIn(
    uintptr_t start, size_t size, uintptr_t target, uintptr_t hook)
{
    uintptr_t site;
    MUST(M_FindCall(start, size, target, &site));
    return Patch_Call(site, target, hook);
}

static RESULT M_PatchValueIn(
    uintptr_t start, size_t size, uint32_t value, uintptr_t hook)
{
    uintptr_t site;
    MUST(M_FindValue32(start, size, value, &site));
    return Patch_Value32(site, value, (uint32_t)hook);
}

static void M_Attach(void)
{
    // Without a safe work directory the game would write into its own.
    wchar_t work[MAX_PATH];
    wchar_t game[MAX_PATH];
    if (!M_GetEnv(L"TH07CAPTURE_WORK", work, MAX_PATH) || work[1] != L':'
        || GetCurrentDirectoryW(MAX_PATH, game) == 0 || M_IsUnder(game, work)) {
        TerminateProcess(GetCurrentProcess(), 1);
    }
    M_Patch(Overlay_Install(game, work));
    wchar_t flag[4];
    const uint32_t step =
        M_GetEnv(L"TH07CAPTURE_FAST", flag, 4) ? FAST_CLOCK_STEP : 0;
    if (!Clock_Install(GetModuleHandleW(nullptr), step)) {
        LOG_ERROR("no clock function is imported");
        TerminateProcess(GetCurrentProcess(), 1);
    }
    M_Patch(M_PatchCallIn(
        RETAIL_RUN_FRAME, RETAIL_RUN_FRAME_SIZE, RETAIL_RUN_CALC_CHAIN,
        (uintptr_t)M_RunCalcChain));

    wchar_t replay[MAX_PATH];
    if (M_GetEnv(L"TH07CAPTURE_REPLAY", replay, MAX_PATH)) {
        WideCharToMultiByte(
            CP_ACP, 0, replay, -1, m_ReplayPath, sizeof(m_ReplayPath), nullptr,
            nullptr);
        M_Patch(M_PatchValueIn(
            RETAIL_SET_UP_TITLE_SCENE, RETAIL_SET_UP_TITLE_SCENE_SIZE,
            RETAIL_MAIN_MENU_ON_UPDATE, (uintptr_t)M_MainMenuOnUpdate));
        M_Patch(M_PatchValueIn(
            RETAIL_REGISTER_REPLAY_CHAIN, RETAIL_REGISTER_REPLAY_CHAIN_SIZE,
            RETAIL_PLAY_ON_UPDATE_2, (uintptr_t)M_PlayOnUpdate2));
    } else if (M_GetEnv(L"TH07CAPTURE_RESULT", flag, 4)) {
        M_Patch(M_PatchValueIn(
            RETAIL_SET_UP_TITLE_SCENE, RETAIL_SET_UP_TITLE_SCENE_SIZE,
            RETAIL_MAIN_MENU_ON_UPDATE, (uintptr_t)M_MainMenuOpenResult));
    }

    wchar_t quit_after[16];
    if (M_GetEnv(L"TH07CAPTURE_QUIT_AFTER", quit_after, 16)) {
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
