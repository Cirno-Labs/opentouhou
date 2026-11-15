#include "th07/stubs.h"

#include "core/base/log.h"

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

RESULT Bullet_Register(void)
{
    LOG_INFO("Bullet_Register is not ported");
    return OK;
}

void Bullet_Remove(void)
{
    LOG_INFO("Bullet_Remove is not ported");
}

RESULT Enemy_Register(void)
{
    LOG_INFO("Enemy_Register is not ported");
    return OK;
}

void Enemy_Remove(void)
{
    LOG_INFO("Enemy_Remove is not ported");
}

RESULT Gui_Register(void)
{
    LOG_INFO("Gui_Register is not ported");
    return OK;
}

void Gui_Remove(void)
{
    LOG_INFO("Gui_Remove is not ported");
}

RESULT Ecl_Load(void)
{
    LOG_INFO("Ecl_Load is not ported");
    return OK;
}

void Ecl_Release(void)
{
    LOG_INFO("Ecl_Release is not ported");
}

bool Gui_IsMsgRunning(void)
{
    return false;
}

bool Gui_IsMsgSkippable(void)
{
    return false;
}

void Gui_ShowCherryFullPopup(const int32_t cherry)
{
    LOG_INFO("Gui_ShowCherryFullPopup is not ported");
}

void Gui_ShowFullPowerPopup(void)
{
    LOG_INFO("Gui_ShowFullPowerPopup is not ported");
}

void Gui_ShowBorderStartPopup(void)
{
    LOG_INFO("Gui_ShowBorderStartPopup is not ported");
}

void Gui_ShowBorderBonusPopup(const int32_t points)
{
    LOG_INFO("Gui_ShowBorderBonusPopup is not ported");
}

void Gui_ShowBombName(const int32_t sprite, const char *const name)
{
    LOG_INFO("Gui_ShowBombName is not ported");
}

void Gui_EndBombName(void)
{
    LOG_INFO("Gui_EndBombName is not ported");
}

void Bullet_ClearAll(void)
{
}

void Bullet_ClearAllToItems(void)
{
}

void Bullet_StopAll(void)
{
    LOG_INFO("Bullet_StopAll is not ported");
}

bool Enemy_IsBossActive(void)
{
    return false;
}

int32_t Bullet_Count(void)
{
    return 0;
}

RESULT ResultScreen_LoadGameRecords(void)
{
    LOG_INFO("ResultScreen_LoadGameRecords is not ported");
    return OK;
}

ENEMIES g_Enemies;
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

void PlayTime_Stop(void)
{
    LOG_INFO("PlayTime_Stop is not ported");
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
