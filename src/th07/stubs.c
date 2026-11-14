#include "th07/stubs.h"

#include "core/base/log.h"

GAME_MANAGER g_GameManager;

RESULT TitleScene_SetUp(const int32_t mode)
{
    LOG_INFO("TitleScene_SetUp is not ported");
    return OK;
}

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

PLAYER g_Player;
GUI g_Gui;

void GameManager_RerollChecksumKeys(void)
{
    LOG_INFO("GameManager_RerollChecksumKeys is not ported");
}

void GameManager_SetBombs(const int32_t bombs)
{
    LOG_INFO("GameManager_SetBombs is not ported");
}

void Sound_Queue(const int32_t sound)
{
    LOG_INFO("Sound_Queue is not ported");
}

void Sound_PauseMusic(void)
{
    LOG_INFO("Sound_PauseMusic is not ported");
}

void Sound_ResumeMusic(void)
{
    LOG_INFO("Sound_ResumeMusic is not ported");
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
