#include "th07/stubs.h"

#include "th07/ecl.h"
#include "th07/game_manager.h"

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

RESULT Gui_Register(void)
{
    LOG_INFO("Gui_Register is not ported");
    return OK;
}

void Gui_Remove(void)
{
    LOG_INFO("Gui_Remove is not ported");
}

void Ecl_StartSub(ECL_STATE *const state, const int16_t sub)
{
}

ECL_RUN Ecl_RunEnemy(ENEMY *const enemy)
{
    return ECL_RUN_CONTINUE;
}

bool Gui_IsMsgRunning(void)
{
    return false;
}

bool Gui_IsMsgSkippable(void)
{
    return false;
}

void Gui_StartMsg(const int32_t index)
{
    LOG_INFO("Gui_StartMsg is not ported");
}

bool Gui_MsgHoldsTimeline(void)
{
    return false;
}

void Gui_ShowBonusPopup(const int32_t bonus)
{
    LOG_INFO("Gui_ShowBonusPopup is not ported");
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

void Gui_ShowSpellCardName(const int32_t sprite, const char *const name)
{
    LOG_INFO("Gui_ShowSpellCardName is not ported");
}

void Gui_EndSpellCardName(void)
{
    LOG_INFO("Gui_EndSpellCardName is not ported");
}

void Gui_ShowSpellCardBonus(const int32_t bonus)
{
    LOG_INFO("Gui_ShowSpellCardBonus is not ported");
}

void ResultScreen_AddSpellCardAttempt(
    const int32_t index, const char *const name)
{
    LOG_INFO("ResultScreen_AddSpellCardAttempt is not ported");
}

void ResultScreen_AddSpellCardCapture(const int32_t index, const int32_t bonus)
{
    int32_t *const captures = &g_GameManager.records.spell_captures[index];
    if (*captures < 9999) {
        (*captures)++;
    }
}

RESULT ResultScreen_LoadGameRecords(void)
{
    LOG_INFO("ResultScreen_LoadGameRecords is not ported");
    return OK;
}

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
