#include "th07/game_manager.h"

#include "th07/gui.h"
#include "th07/player.h"

#define M_BORDER_PLUS 50000
#define M_MAX_LIMIT 9999990

// Returns the cherry before the add.
static int32_t M_Add(const int32_t amount)
{
    const int32_t previous = g_GameManager.cherry.value;
    g_GameManager.cherry.value += amount;
    if (g_GameManager.cherry.value > g_GameManager.cherry.max) {
        g_GameManager.cherry.value = g_GameManager.cherry.max;
    }
    return previous;
}

static void M_ShowFull(const int32_t previous)
{
    if (g_GameManager.cherry.value >= g_GameManager.cherry.max
        && previous != g_GameManager.cherry.value) {
        Gui_ShowCherryFullPopup(
            g_GameManager.cherry.value - g_GameManager.cherry.offset);
    }
}

void GameManager_AddCherry(const int32_t amount)
{
    const int32_t previous = M_Add(amount);
    if (amount > 0 && g_Player.border_state == PLAYER_BORDER_NONE) {
        const int32_t full = g_GameManager.cherry.offset + M_BORDER_PLUS;
        g_GameManager.cherry.plus += amount;
        if (g_GameManager.cherry.plus >= full) {
            g_GameManager.cherry.plus = full;
            Player_StartBorder();
        }
    }
    M_ShowFull(previous);
}

void GameManager_AddCherryNoPlus(const int32_t amount)
{
    M_ShowFull(M_Add(amount));
}

void GameManager_AddCherryQuiet(const int32_t amount)
{
    M_Add(amount);
}

void GameManager_RaiseCherryMax(const int32_t amount)
{
    const int32_t limit = g_GameManager.cherry.offset + M_MAX_LIMIT;
    g_GameManager.cherry.max += amount;
    if (g_GameManager.cherry.max >= limit) {
        g_GameManager.cherry.max = limit;
    }
}

void GameManager_SubCherry(const int32_t amount)
{
    if (g_GameManager.cherry.value - g_GameManager.cherry.offset >= amount) {
        g_GameManager.cherry.value -= amount;
    } else {
        g_GameManager.cherry.value = g_GameManager.cherry.offset;
    }
}
