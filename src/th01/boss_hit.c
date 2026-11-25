#include "th01/boss_hit.h"
#include "th01/session.h"
#include "th01/sound.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

static void M_Flash(const uint8_t *const colors, const uint8_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        ASSERT(colors[i] < 16);
        Palette_SetColor(&g_Palette, colors[i], 15, 15, 15);
    }
}

void BossHit_UpdateAndRender(
    BOSS_HIT *const hit, int16_t *const hp, const PALETTE *const boss_palette,
    const uint8_t *const flash_colors, const uint8_t flash_count,
    const int16_t hit_score, const bool orb_hit, const BOSS_SHOT_HITBOX box)
{
    if (g_TestDamage) {
        *hp = Math16_Wrap(*hp - 1);
        g_TestDamage = false;
        HudHp_Decrement(*hp);
    }
    Shots_HitTestBoss(
        box.left, box.top, box.width_minus_shot, box.height_minus_shot);
    if ((orb_hit && !hit->invincible)
        || Bomb_DealsDamage(g_Session.lives, g_Session.frame_rand)) {
        hit->frame = 0;
        M_Flash(flash_colors, flash_count);
        *hp = Math16_Wrap(*hp - 1);
        hit->invincible = true;
        const ORB *const orb = g_Orb;
        if (orb->velocity_x == ORB_X_LEFT_4
            || orb->velocity_x == ORB_X_LEFT_8) {
            Orb_SetVelocityX(ORB_X_RIGHT_4);
        } else if (
            orb->velocity_x == ORB_X_RIGHT_4
            || orb->velocity_x == ORB_X_RIGHT_8) {
            Orb_SetVelocityX(ORB_X_LEFT_4);
        } else if (orb->velocity_x == ORB_X_STILL) {
            Orb_SetVelocityX(orb->left < 304 ? ORB_X_RIGHT_4 : ORB_X_LEFT_4);
        }
        Stage_AddScore((uint32_t)(int32_t)hit_score);
        Hud_ScoreAndCombo();
        Sound_Effect(3);
        HudHp_Decrement(*hp);
    }
    if (hit->invincible && hit->frame < 40) {
        if (hit->frame % 4 == 2) {
            M_Flash(flash_colors, flash_count);
        } else if (hit->frame % 4 == 0) {
            Palette_Set(&g_Palette, boss_palette);
        }
    } else if (hit->frame > 40) {
        Palette_Set(&g_Palette, boss_palette);
        hit->frame = 0;
        hit->invincible = false;
    }
}
