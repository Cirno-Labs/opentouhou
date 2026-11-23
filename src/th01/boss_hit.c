#include "th01/boss_hit.h"

#include "core/base/assert.h"
#include "pc98/zun/math16.h"

static void M_Flash(
    const BOSS_HIT_CONTEXT *const c, const uint8_t *const colors,
    const uint8_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        ASSERT(colors[i] < 16);
        Palette_SetColor(c->palette, c->display->video, colors[i], 15, 15, 15);
    }
}

void BossHit_UpdateAndRender(
    BOSS_HIT *const hit, const BOSS_HIT_CONTEXT *const c,
    const uint8_t *const flash_colors, const uint8_t flash_count,
    const int16_t hit_score, const bool orb_hit, const BOSS_SHOT_HITBOX box)
{
    if (*c->test_damage) {
        *c->hp = Math16_Wrap(*c->hp - 1);
        *c->test_damage = false;
        HudHp_Decrement(c->health, c->display->video, *c->hp);
    }
    Shots_HitTestBoss(
        c->shots, box.left, box.top, box.width_minus_shot,
        box.height_minus_shot, c->shots_output);
    if ((orb_hit && !hit->invincible)
        || Bomb_DealsDamage(
            c->display->player, *c->display->remaining_lives, c->frame_rand)) {
        hit->frame = 0;
        M_Flash(c, flash_colors, flash_count);
        *c->hp = Math16_Wrap(*c->hp - 1);
        hit->invincible = true;
        ORB *const orb = c->orb;
        if (orb->velocity_x == ORB_X_LEFT_4
            || orb->velocity_x == ORB_X_LEFT_8) {
            orb->velocity_x = ORB_X_RIGHT_4;
        } else if (
            orb->velocity_x == ORB_X_RIGHT_4
            || orb->velocity_x == ORB_X_RIGHT_8) {
            orb->velocity_x = ORB_X_LEFT_4;
        } else if (orb->velocity_x == ORB_X_STILL) {
            orb->velocity_x = orb->left < 304 ? ORB_X_RIGHT_4 : ORB_X_LEFT_4;
        }
        c->display->gameplay->score += (uint32_t)(int32_t)hit_score;
        Hud_ScoreAndCombo(c->hud, c->display);
        c->effect(c->user, 3);
        HudHp_Decrement(c->health, c->display->video, *c->hp);
    }
    if (hit->invincible && hit->frame < 40) {
        if (hit->frame % 4 == 2) {
            M_Flash(c, flash_colors, flash_count);
        } else if (hit->frame % 4 == 0) {
            Palette_Set(c->palette, c->display->video, c->boss_palette);
        }
    } else if (hit->frame > 40) {
        Palette_Set(c->palette, c->display->video, c->boss_palette);
        hit->frame = 0;
        hit->invincible = false;
    }
}
