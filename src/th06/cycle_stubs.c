// Stands in for the gameplay modules that are not ported yet, so the
// modules that are can build and run their tests. Each module removes its
// part when it lands.

#include "th06/enemy.h"
#include "th06/gui.h"

// enemy

ENEMY_MANAGER g_Enemies;

RESULT Enemy_Register(void)
{
    return OK;
}

void Enemy_Remove(void)
{
}

ENEMY *Enemy_Spawn(
    const int32_t sub, const VEC3 pos, const int16_t life, const int16_t item,
    const int32_t score)
{
    static ENEMY spare;
    return &spare;
}

void Enemy_ClampPos(ENEMY *const enemy)
{
}

void Enemy_ReleaseOrbits(ENEMY *const enemy)
{
}

void Enemy_KillAll(void)
{
}

// gui

GUI g_Gui;

RESULT Gui_Register(void)
{
    return OK;
}

void Gui_Remove(void)
{
}

void Gui_StartMsg(const int32_t index)
{
}

bool Gui_IsMsgRunning(void)
{
    return false;
}

bool Gui_MsgHoldsTimeline(void)
{
    return false;
}

bool Gui_IsMsgSkippable(void)
{
    return false;
}

bool Gui_IsLoadingShown(void)
{
    return false;
}

void Gui_ShowBombName(const int32_t sprite, const char *const name)
{
}

void Gui_EndBombName(void)
{
}

void Gui_ShowSpellCardName(const int32_t sprite, const char *const name)
{
}

void Gui_EndSpellCardName(void)
{
}

void Gui_ShowSpellCardBonus(const int32_t bonus)
{
}

void Gui_ShowBonusPopup(const int32_t bonus)
{
}

void Gui_ShowFullPowerPopup(void)
{
}
