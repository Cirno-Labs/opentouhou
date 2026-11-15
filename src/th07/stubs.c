#include "th07/stubs.h"

#include "core/base/log.h"

RESULT GameManager_Register(void)
{
    LOG_INFO("GameManager_Register is not ported");
    return OK;
}

void GameManager_Remove(void)
{
    LOG_INFO("GameManager_Remove is not ported");
}

RESULT ResultScreen_Register(const int32_t mode)
{
    LOG_INFO("ResultScreen_Register is not ported");
    return OK;
}

RESULT MusicScreen_Register(void)
{
    LOG_INFO("MusicScreen_Register is not ported");
    return OK;
}

RESULT Ending_Register(void)
{
    LOG_INFO("Ending_Register is not ported");
    return OK;
}

void Replay_Discard(void)
{
    LOG_INFO("Replay_Discard is not ported");
}

void Player_StartBorder(void)
{
    LOG_INFO("Player_StartBorder is not ported");
}

void Gui_ShowCherryFullPopup(const int32_t cherry)
{
    LOG_INFO("Gui_ShowCherryFullPopup is not ported");
}

PLAYER g_Player;
GUI g_Gui;
uint8_t g_EffectsQuality = 2;

void ResultScreen_LoadTitleRecords(void)
{
    LOG_INFO("ResultScreen_LoadTitleRecords is not ported");
}

void PlayTime_Add(void)
{
    LOG_INFO("PlayTime_Add is not ported");
}

void PlayTime_Restart(void)
{
    LOG_INFO("PlayTime_Restart is not ported");
}

bool Anm_SetPendingCopy(
    const int32_t slot, const GFX_RECT source, const GFX_RECT dest)
{
    LOG_INFO("Anm_SetPendingCopy is not ported");
    return true;
}

bool Gui_IsLoadingShown(void)
{
    return false;
}
