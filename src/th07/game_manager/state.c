#include "th07/game_manager.h"

#include "th07/game.h"

#include "core/base/utils.h"

#include <string.h>

#define M_SHOT_COUNT 6
#define M_PHANTASM_CAPTURES 60

GAME_MANAGER g_GameManager;

static int32_t M_SpellCardsCaptured(void)
{
    int32_t count = 0;
    for (size_t i = 0; i < ARRAY_SIZE(g_GameManager.records.spell_cards); i++) {
        if (g_GameManager.records.spell_cards[i].captures[SCORE_ALL_SHOTS]
            > 0) {
            count++;
        }
    }
    return count;
}

void GameManager_Create(GAME_MANAGER *const gm)
{
    memset(gm, 0, sizeof(*gm));
    gm->play_area = (PLAY_RECT) { 32.0f, 16.0f, 384.0f, 448.0f };
    gm->mode.demo_index = 2;
    gm->mode.replay_fps = 60;
    gm->records.phantasm_was_open = true;
}

bool GameManager_IsInPlayArea(
    const float x, const float y, const float width, const float height)
{
    // Negated comparisons keep a NaN coordinate inside, as in the original.
    return !(x + width / 2.0f < 0.0f) && !(x - width / 2.0f > 384.0f)
        && !(y + height / 2.0f < 0.0f) && !(y - height / 2.0f > 448.0f);
}

bool GameManager_HasExtraOpen(const int32_t shot)
{
    const uint8_t *const cleared =
        g_GameManager.records.clears[shot].no_continues;
    return cleared[0] == 99 || cleared[1] == 99 || cleared[2] == 99
        || cleared[3] == 99;
}

bool GameManager_AnyShotHasExtraOpen(void)
{
    for (int32_t shot = 0; shot < M_SHOT_COUNT; shot++) {
        if (GameManager_HasExtraOpen(shot)) {
            return true;
        }
    }
    return false;
}

bool GameManager_HasPhantasmOpen(const int32_t shot)
{
    uint8_t *const cleared = g_GameManager.records.clears[shot].no_continues;
    if (M_SpellCardsCaptured() >= M_PHANTASM_CAPTURES && cleared[4] == 99) {
        cleared[5] = 99;
    }
    return cleared[5] == 99;
}

bool GameManager_IsPhantasmOpen(void)
{
    if (M_SpellCardsCaptured() >= M_PHANTASM_CAPTURES) {
        for (int32_t shot = 0; shot < M_SHOT_COUNT; shot++) {
            uint8_t *const cleared =
                g_GameManager.records.clears[shot].no_continues;
            if (cleared[4] == 99) {
                cleared[5] = 99;
            }
        }
    }
    for (int32_t shot = 0; shot < M_SHOT_COUNT; shot++) {
        if (g_GameManager.records.clears[shot].no_continues[5] == 99) {
            return true;
        }
    }
    return false;
}

void GameManager_AddScore(const int32_t points)
{
    g_GameManager.score.value += (uint32_t)(points / 10);
}

void GameManager_DrawChecksumKeys(void)
{
    Rng_U32InRange(&g_Rng, 100000);
    Rng_U32InRange(&g_Rng, 100000);
}

void GameManager_DrawLifeChecksumKeys(void)
{
    for (int32_t i = 0; i < 5; i++) {
        Rng_U32InRange(&g_Rng, 100000);
    }
    for (int32_t i = 0; i < 3; i++) {
        Rng_F32(&g_Rng);
    }
}

void GameManager_AddLives(const int32_t lives)
{
    g_GameManager.player.lives += (float)lives;
    GameManager_DrawChecksumKeys();
}

void GameManager_AddBombs(const int32_t bombs)
{
    g_GameManager.player.bombs += (float)bombs;
    GameManager_DrawChecksumKeys();
}

void GameManager_AddPower(const int32_t power)
{
    g_GameManager.player.power += (float)power;
    GameManager_DrawChecksumKeys();
}

void GameManager_AddDeaths(const int32_t deaths)
{
    g_GameManager.stats.deaths += (float)deaths;
    GameManager_DrawChecksumKeys();
}

void GameManager_AddBombsUsed(const int32_t bombs)
{
    g_GameManager.stats.bombs_used += (float)bombs;
    GameManager_DrawChecksumKeys();
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
