#include "../core/capture.h"
#include "../core/patch.h"
#include "fields.h"
#include "retail.h"
#include "state.h"

#include "core/base/log.h"
#include "core/base/utils.h"

#include <windows.h>

#include <string.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
typedef int(__attribute__((thiscall)) * RUN_CALC_CHAIN_FN)(void *chain);
#pragma GCC diagnostic pop

static const char *const m_StartNames[] = { TH06_START_NAMES };
static const char *const m_CalcNames[] = { TH06_CALC_NAMES };

static const CAPTURE_CASE m_Capture = {
    .game_id = "th06",
    .info = "th06 1.02h",
    .start_names = m_StartNames,
    .start_count = ARRAY_SIZE(m_StartNames),
    .calc_names = m_CalcNames,
    .calc_count = ARRAY_SIZE(m_CalcNames),
    .start = State_Start,
    .calc = State_Calc,
};

static char m_ReplayPath[MAX_PATH];
static bool m_ReplayStarted;
static bool m_ReplayEnded;
static bool m_ResultOpened;

static void M_EndOfCalc(void)
{
    Capture_CountCalc();
    if (RETAIL_GAME_MANAGER_AT->is_playing != 0
        && RETAIL_GAME_MANAGER_AT->is_in_demo == 0) {
        Capture_OpenCase();
    }
    Capture_WriteRow();
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

static bool M_TitleDelayReached(const void *const menu)
{
    const int state = ((const RETAIL_MAIN_MENU *)menu)->state;
    return Capture_TitleDelayReached(
        state == RETAIL_MAIN_MENU_TITLE || state == RETAIL_MAIN_MENU_TITLE_KEY);
}

static int __cdecl M_MainMenuOnUpdate(void *menu)
{
    if (m_ReplayStarted) {
        if (!m_ReplayEnded) {
            m_ReplayEnded = true;
            Capture_CloseGame();
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

static void M_Attach(void)
{
    const CAPTURE_MODE mode =
        Capture_Attach(&m_Capture, m_ReplayPath, sizeof(m_ReplayPath));
    Capture_Check(Patch_Call(
        RETAIL_RUN_CALC_CHAIN_CALL, RETAIL_RUN_CALC_CHAIN,
        (uintptr_t)M_RunCalcChain));
    if (mode == CAPTURE_REPLAY) {
        Capture_Check(Patch_Value32(
            RETAIL_TITLE_CALLBACK_SITE, RETAIL_MAIN_MENU_ON_UPDATE,
            (uint32_t)(uintptr_t)M_MainMenuOnUpdate));
        Capture_Check(Patch_Value32(
            RETAIL_PLAY_ON_UPDATE_2_SITE, RETAIL_PLAY_ON_UPDATE_2,
            (uint32_t)(uintptr_t)M_PlayOnUpdate2));
    } else if (mode == CAPTURE_RESULT) {
        Capture_Check(Patch_Value32(
            RETAIL_TITLE_CALLBACK_SITE, RETAIL_MAIN_MENU_ON_UPDATE,
            (uint32_t)(uintptr_t)M_MainMenuOpenResult));
    }
    LOG_INFO("dll: ready");
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        M_Attach();
    } else if (reason == DLL_PROCESS_DETACH) {
        Capture_CloseCase();
    }
    return TRUE;
}
