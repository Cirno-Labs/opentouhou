#include "th01/shots.h"

#include "pc98/zun/math16.h"
#include "th01/sprite.h"

#include <string.h>

SHOTS g_Shots;

void Shots_Init(void)
{
    memset(&g_Shots, 0, sizeof(g_Shots));
}

static void M_Action(
    const SHOTS_OUTPUT output, const SHOTS_ACTION_KIND kind,
    const SHOT *const shot)
{
    output.action(
        output.user,
        (SHOTS_ACTION) {
            .kind = kind,
            .left = shot->left,
            .top = shot->top,
            .quarter = shot->moving ? 0 : shot->decay_frame / 4 + 1,
        });
}

static void M_Effect(const SHOTS_OUTPUT output, const int16_t effect)
{
    output.action(
        output.user,
        (SHOTS_ACTION) {
            .kind = SHOTS_ACTION_EFFECT,
            .effect = effect,
        });
}

static void M_Hit(SHOT *const shot, const SHOTS_OUTPUT output)
{
    M_Action(output, SHOTS_ACTION_RESTORE, shot);
    shot->moving = false;
    shot->decay_frame = 1;
    M_Effect(output, 16);
}

RESULT Shots_ValidateSprites(const PTN *const sprites)
{
    FAIL_IF(
        sprites->count <= SHOTS_PTN || sprites->pixels == nullptr,
        "Incomplete shot PTN sprites");
    return OK;
}

bool Shots_Add(const int16_t left, const int16_t top, const SHOTS_OUTPUT output)
{
    if (left < 0 || left > 639) {
        return false;
    }
    for (int32_t i = 0; i < SHOTS_COUNT; i++) {
        SHOT *const shot = &g_Shots.slots[i];
        if (!shot->moving && shot->decay_frame == 0) {
            shot->left = left;
            shot->top = top;
            shot->moving = true;
            M_Effect(output, 1);
            shot->decay_frame = 0;
            return true;
        }
    }
    return false;
}

void Shots_EraseAndReset(const SHOTS_OUTPUT output)
{
    for (int32_t i = 0; i < SHOTS_COUNT; i++) {
        SHOT *const shot = &g_Shots.slots[i];
        if (shot->moving) {
            M_Action(output, SHOTS_ACTION_RESTORE, shot);
            shot->moving = false;
        } else if (shot->decay_frame != 0) {
            M_Action(output, SHOTS_ACTION_RESTORE, shot);
            shot->decay_frame = 0;
        }
    }
}

bool Shots_HitTestOrb(const int32_t slot)
{
    SHOT *const shot = &g_Shots.slots[slot];
    const int16_t dx = Math16_Wrap(shot->left - g_Orb.left);
    const int16_t dy = Math16_Wrap(shot->top - g_Orb.top);
    if (!shot->moving || dx >= 32 || dx <= -16 || dy >= 32 || dy <= -16) {
        return false;
    }
    if (dx > 8) {
        g_Orb.velocity_x = ORB_X_LEFT_4;
    } else if (dx == 8) {
        g_Orb.velocity_x = ORB_X_STILL;
    } else if (dx > -16) {
        g_Orb.velocity_x = ORB_X_RIGHT_4;
    }
    Orb_NewForce(ORB_FORCE_SHOT, 0);
    shot->moving = false;
    Orb_MoveX(g_Orb.velocity_x);
    g_Orb.top = Math16_Wrap(g_Orb.top + Orb_UpdateVelocityY());
    return true;
}

void Shots_UpdateAndRender(const SHOTS_OUTPUT output)
{
    for (int32_t i = 0; i < SHOTS_COUNT; i++) {
        SHOT *const shot = &g_Shots.slots[i];
        if (shot->moving) {
            M_Action(output, SHOTS_ACTION_RESTORE, shot);
            shot->top = Math16_Wrap(shot->top - 12);
            if (!g_Orb.in_portal && Shots_HitTestOrb(i)) {
                continue;
            }
            if (shot->top >= 64) {
                M_Action(output, SHOTS_ACTION_PUT, shot);
            } else {
                shot->moving = false;
            }
        } else if (shot->decay_frame != 0) {
            M_Action(output, SHOTS_ACTION_ERASE, shot);
            shot->decay_frame++;
            if (shot->decay_frame > 7) {
                shot->decay_frame = 0;
                shot->moving = false;
            } else {
                M_Action(output, SHOTS_ACTION_PUT, shot);
            }
        }
    }
}

bool Shots_HitTestPellet(
    const int16_t left, const int16_t top, const SHOTS_OUTPUT output)
{
    for (int32_t i = 0; i < SHOTS_COUNT; i++) {
        SHOT *const shot = &g_Shots.slots[i];
        const int16_t dx = Math16_Wrap(shot->left - left);
        const int16_t dy = Math16_Wrap(shot->top - top);
        if (shot->moving && shot->decay_frame != 1 && dx <= 6 && dx >= -14
            && dy <= 6 && dy >= -14) {
            M_Hit(shot, output);
            return true;
        }
    }
    return false;
}

bool Shots_HitTestBoss(
    const int16_t left, const int16_t top, const int16_t width_minus_shot,
    const int16_t height_minus_shot, const SHOTS_OUTPUT output)
{
    for (int32_t i = 0; i < SHOTS_COUNT; i++) {
        SHOT *const shot = &g_Shots.slots[i];
        if (shot->moving && shot->left >= left && shot->top >= top
            && shot->left <= Math16_Wrap(left + width_minus_shot)
            && shot->top <= Math16_Wrap(top + height_minus_shot)) {
            M_Hit(shot, output);
            return true;
        }
    }
    return false;
}

void Shots_RenderAction(const PTN *const sprites, const SHOTS_ACTION action)
{
    switch (action.kind) {
    case SHOTS_ACTION_RESTORE:
        Sprite_Restore16(action.left, action.top, 16, 16);
        break;
    case SHOTS_ACTION_PUT:
        Sprite_PtnPut(
            sprites, SHOTS_PTN, action.quarter, action.left, action.top);
        break;
    case SHOTS_ACTION_ERASE:
        Sprite_PtnErase(
            sprites, SHOTS_PTN, action.quarter, action.left, action.top);
        break;
    case SHOTS_ACTION_EFFECT:
        break;
    }
}
