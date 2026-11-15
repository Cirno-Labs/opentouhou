#include "../core/capture.h"
#include "../core/patch.h"
#include "fields.h"
#include "retail.h"
#include "state.h"

#include "core/base/log.h"
#include "core/base/result.h"
#include "core/base/utils.h"

#include <windows.h>

#include <string.h>

#define CALL_OPCODE 0xe8
#define CALL_SIZE 5

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
typedef int(__attribute__((thiscall)) * RUN_CALC_CHAIN_FN)(void *chain);
#pragma GCC diagnostic pop

static void M_CaseClosed(uint32_t rows);

static const char *const m_StartNames[] = { TH07_START_NAMES };
static const char *const m_CalcNames[] = { TH07_CALC_NAMES };

static const CAPTURE_CASE m_Capture = {
    .game_id = "th07",
    .info = "th07 1.00b",
    .start_names = m_StartNames,
    .start_count = ARRAY_SIZE(m_StartNames),
    .calc_names = m_CalcNames,
    .calc_count = ARRAY_SIZE(m_CalcNames),
    .start = State_Start,
    .calc = State_Calc,
    .closed = M_CaseClosed,
};

static char m_ReplayPath[MAX_PATH];
static bool m_ReplayStarted;
static bool m_ReplayEnded;
static bool m_ResultOpened;
static uint32_t m_FastForwards;

static void M_CaseClosed(const uint32_t rows)
{
    LOG_INFO("case: closed, %u rows, %u fast-forwards", rows, m_FastForwards);
}

// Writes rows while the game plays, from the first calc of play (not the
// title's demo) to the last one before it leaves the gameplay scene.
// The title scene gives GameManager new, uninitialized stats.
static void M_EndOfCalc(void)
{
    Capture_CountCalc();
    const RETAIL_GAME_MANAGER *const game = RETAIL_GAME_MANAGER_AT;
    const bool gameplay = RETAIL_APP_AT->cur_scene == RETAIL_SCENE_GAMEPLAY;
    if (!gameplay) {
        Capture_CloseCase();
    }
    if (gameplay && (game->state_flags & RETAIL_STATE_PLAYING) != 0
        && (game->state_flags & RETAIL_STATE_DEMO) == 0) {
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

static bool M_TitleDelayReached(const void *const menu)
{
    const RETAIL_MAIN_MENU *const main_menu = menu;
    return Capture_TitleDelayReached(
        main_menu->state == RETAIL_MAIN_MENU_TITLE
        && main_menu->sub_state == RETAIL_MAIN_MENU_TITLE_SUB_STATE);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
static int __attribute__((fastcall)) M_MainMenuOnUpdate(void *menu)
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
    const CAPTURE_MODE mode =
        Capture_Attach(&m_Capture, m_ReplayPath, sizeof(m_ReplayPath));
    Capture_Check(M_PatchCallIn(
        RETAIL_RUN_FRAME, RETAIL_RUN_FRAME_SIZE, RETAIL_RUN_CALC_CHAIN,
        (uintptr_t)M_RunCalcChain));
    if (mode == CAPTURE_REPLAY) {
        Capture_Check(M_PatchValueIn(
            RETAIL_SET_UP_TITLE_SCENE, RETAIL_SET_UP_TITLE_SCENE_SIZE,
            RETAIL_MAIN_MENU_ON_UPDATE, (uintptr_t)M_MainMenuOnUpdate));
        Capture_Check(M_PatchValueIn(
            RETAIL_REGISTER_REPLAY_CHAIN, RETAIL_REGISTER_REPLAY_CHAIN_SIZE,
            RETAIL_PLAY_ON_UPDATE_2, (uintptr_t)M_PlayOnUpdate2));
    } else if (mode == CAPTURE_RESULT) {
        Capture_Check(M_PatchValueIn(
            RETAIL_SET_UP_TITLE_SCENE, RETAIL_SET_UP_TITLE_SCENE_SIZE,
            RETAIL_MAIN_MENU_ON_UPDATE, (uintptr_t)M_MainMenuOpenResult));
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
