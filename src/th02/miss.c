#include "th02/hud.h"
#include "th02/items.h"
#include "th02/miss_private.h"
#include "th02/performance.h"
#include "th02/player.h"
#include "th02/score.h"
#include "th02/shots.h"
#include "th02/sound_effects.h"
#include "th02/sparks.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

#include <string.h>

#define MISS_INVINCIBILITY_FRAMES 220

static MISS m_Miss;
const MISS *const g_Miss = &m_Miss;
MISS *const p_Miss = &m_Miss;

static const uint8_t m_Power[] = { 1, 1, 4, 8, 16, 24, 32, 40, 52, 64 };

static void M_Sparks(const uint8_t back, const int16_t count)
{
    const ENTITY_POINT position = g_Player->position[back];
    Sparks_Add(
        back, Math16_Wrap((int32_t)position.x + 16),
        (uint16_t)(position.y + 24), 128, count, true);
}

static void M_Items(const uint8_t back)
{
    const ENTITY_POINT position = g_Player->position[back];
    Items_Drop(position.x, position.y);
}

void Miss_Init(void)
{
    memset(&m_Miss, 0, sizeof(m_Miss));
}

void Miss_ResetStage(void)
{
    m_Miss.stage_misses = 0;
    m_Miss.frame = 0;
    m_Miss.active = 0;
}

void Miss_ClearGameOver(void)
{
    m_Miss.item_gameover = 0;
}

void Miss_Update(
    const PLAYFIELD_PAGES pages, const PATTERN_BANK *const patterns,
    const uint8_t start_bombs)
{
    ASSERT(pages.back < 2);

    if (m_Miss.frame == 0) {
        SoundEffects_Play(SOUND_EFFECT_BOSS_HEAVY_HIT);
        m_Miss.active = 1;
        m_Miss.stage_misses++;
        Score_Commit();
        Performance_CountMiss();
        M_Sparks(pages.back, 16);
    }
    Player_SyncCurrent(pages.back);
    m_Miss.frame++;
    if (m_Miss.frame < 24) {
        PlayerRender_Put(
            patterns, g_Player->position[pages.back], pages.scroll,
            4 + (m_Miss.frame >> 2), false);
    } else if (m_Miss.frame == 24) {
        Player_BeginInvincibility(MISS_INVINCIBILITY_FRAMES);
        Performance_MissPenalty();
        M_Sparks(pages.back, 32);
        if (g_Score->lives == 0) {
            m_Miss.frame = 0;
            m_Miss.item_gameover = 1;
            M_Items(pages.back);
            m_Miss.item_gameover = 0;
            Player_Kill();
        } else {
            ASSERT(g_Shots->weapon.level < sizeof(m_Power));
            Player_SetPower(m_Power[g_Shots->weapon.level]);
            Shots_SetWeaponLevel(Hud_Power(g_Player->power));
            M_Items(pages.back);
        }
    } else if (m_Miss.frame < 43) {
        Player_MoveMissY(pages.back, 4);
    } else if (m_Miss.frame == 43) {
        Score_LoseLife((int8_t)start_bombs);
        Hud_Lives(g_Score->lives);
        Hud_Bombs(g_Score->bombs);
        M_Sparks(pages.back, 16);
        Player_Respawn();
    } else if (m_Miss.frame < 68) {
        Player_MoveMissY(pages.back, -2);
        PlayerRender_Put(
            patterns, g_Player->position[pages.back], pages.scroll, 0, false);
    } else {
        m_Miss.frame = 0;
        m_Miss.active = 0;
        Player_ClearHit();
    }
}
