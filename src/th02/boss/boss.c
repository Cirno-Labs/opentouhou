#include "th02/boss.h"
#include "th02/gameplay/memory.h"
#include "th02/hooks.h"
#include "th02/shots/hit.h"
#include "th02/palette.h"
#include "th02/score.h"
#include "th02/stage.h"

#include "pc98/zun/math16.h"

#include <string.h>

void Boss_ResetDamage(int16_t *const damage, const int16_t threshold)
{
    *damage = 0;
    if (g_Th02Hooks.on_boss != nullptr) {
        g_Th02Hooks.on_boss(g_Th02Hooks.arg, damage, threshold);
    }
}

void Boss_Init(BOSS_STATE *const boss)
{
    memset(boss, 0, sizeof(*boss));
    boss->angle = 0x20;
}

void Boss_BeginStage(BOSS_STATE *const boss)
{
    boss->phase_frame = 0;
    boss->damage = 0;
    boss->defeated = 0;
    boss->hit_flash = 0;
}

int16_t Boss_Hit(
    int16_t *const total, const int16_t left, const int16_t top,
    const int16_t width, const int16_t height, const int16_t multiplier,
    const uint8_t bomb_frame_mask)
{
    const SHOT_HIT_INPUT input = {
        .frame = g_Stage->world.frame,
        .back = g_Stage->scroll.back,
        .bomb_frame_mask = bomb_frame_mask,
    };
    const int16_t damage = Shots_Hit(input, left, top, width, height);
    Score_AddDamage(damage);
    *total = Math16_Wrap((int32_t)*total + (int32_t)damage * multiplier);
    return damage;
}

void Boss_SelectPatterns(
    PATTERN_BANK *const target, const PATTERN_BANK *const bank,
    const GFX_COLOR *const palette)
{
    GameplayMemory_SelectPatterns(bank);
    memcpy(
        &target->entries[128], &bank->entries[128],
        (PATTERN_LIMIT - 128) * sizeof(PATTERN));
    target->next = bank->next;
    if (palette != nullptr) {
        Palette_Import(palette);
    }
}
