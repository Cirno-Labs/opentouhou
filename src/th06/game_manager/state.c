#include "th06/game_manager.h"

GAME_MANAGER g_GameManager;

void GameManager_Create(GAME_MANAGER *const gm)
{
    *gm = (GAME_MANAGER) {
        .play_area = { 32.0f, 16.0f, 384.0f, 448.0f },
    };
}

bool GameManager_IsExtraUnlocked(const int32_t character, const int32_t shot)
{
    const CLEAR_RECORD *const clear =
        &g_GameManager.records.clears[character * 2 + shot];
    return clear->without_continues[1] == 99
        || clear->without_continues[2] == 99
        || clear->without_continues[3] == 99;
}

bool GameManager_IsInPlayArea(
    const float x, const float y, const float width, const float height)
{
    const PLAY_RECT *const area = &g_GameManager.play_area;
    // Negated comparisons keep a NaN coordinate inside, as in the original.
    return !(x + width / 2.0f < 0.0f) && !(x - width / 2.0f > area->width)
        && !(y + height / 2.0f < 0.0f) && !(y - height / 2.0f > area->height);
}

void GameManager_AddRank(const int32_t amount)
{
    g_GameManager.rank.sub += amount;
    while (g_GameManager.rank.sub >= 100) {
        g_GameManager.rank.value++;
        g_GameManager.rank.sub -= 100;
    }
    if (g_GameManager.rank.value > g_GameManager.rank.max) {
        g_GameManager.rank.value = g_GameManager.rank.max;
    }
}

void GameManager_SubRank(const int32_t amount)
{
    g_GameManager.rank.sub -= amount;
    while (g_GameManager.rank.sub < 0) {
        g_GameManager.rank.value--;
        g_GameManager.rank.sub += 100;
    }
    if (g_GameManager.rank.value < g_GameManager.rank.min) {
        g_GameManager.rank.value = g_GameManager.rank.min;
    }
}
