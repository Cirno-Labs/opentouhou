#include "th01/shots.h"
#include "th01/sound.h"

#include "pc98/zun/math16.h"
#include "th01/sprite.h"

#include <string.h>

static SHOTS m_Shots;
const SHOTS *const g_Shots = &m_Shots;

void Shots_Init(void)
{
    memset(&m_Shots, 0, sizeof(m_Shots));
}

static int16_t M_Quarter(const SHOT *const shot)
{
    return shot->moving ? 0 : (int16_t)(shot->decay_frame / 4 + 1);
}

static void M_Restore(const SHOT *const shot)
{
    Sprite_Restore16(shot->left, shot->top, 16, 16);
}

static void M_PutShot(const PTN *const sprites, const SHOT *const shot)
{
    Sprite_PtnPut(sprites, SHOTS_PTN, M_Quarter(shot), shot->left, shot->top);
}

static void M_Erase(const PTN *const sprites, const SHOT *const shot)
{
    Sprite_PtnErase(sprites, SHOTS_PTN, M_Quarter(shot), shot->left, shot->top);
}

static void M_Hit(SHOT *const shot)
{
    M_Restore(shot);
    shot->moving = false;
    shot->decay_frame = 1;
    Sound_Effect(SOUND_EFFECT_SHOT_HIT);
}

RESULT Shots_ValidateSprites(const PTN *const sprites)
{
    FAIL_IF(
        sprites->count <= SHOTS_PTN || sprites->pixels == nullptr,
        "Incomplete shot PTN sprites");
    return OK;
}

bool Shots_Add(const int16_t left, const int16_t top)
{
    if (left < 0 || left > 639) {
        return false;
    }
    for (int32_t i = 0; i < SHOTS_COUNT; i++) {
        SHOT *const shot = &m_Shots.slots[i];
        if (!shot->moving && shot->decay_frame == 0) {
            shot->left = left;
            shot->top = top;
            shot->moving = true;
            Sound_Effect(SOUND_EFFECT_SHOT);
            shot->decay_frame = 0;
            return true;
        }
    }
    return false;
}

void Shots_EraseAndReset(void)
{
    for (int32_t i = 0; i < SHOTS_COUNT; i++) {
        SHOT *const shot = &m_Shots.slots[i];
        if (shot->moving) {
            M_Restore(shot);
            shot->moving = false;
        } else if (shot->decay_frame != 0) {
            M_Restore(shot);
            shot->decay_frame = 0;
        }
    }
}

bool Shots_HitTestOrb(const int32_t slot)
{
    SHOT *const shot = &m_Shots.slots[slot];
    const int16_t dx = Math16_Wrap(shot->left - g_Orb->left);
    const int16_t dy = Math16_Wrap(shot->top - g_Orb->top);
    if (!shot->moving || dx >= 32 || dx <= -16 || dy >= 32 || dy <= -16) {
        return false;
    }
    if (dx > 8) {
        Orb_SetVelocityX(ORB_X_LEFT_4);
    } else if (dx == 8) {
        Orb_SetVelocityX(ORB_X_STILL);
    } else if (dx > -16) {
        Orb_SetVelocityX(ORB_X_RIGHT_4);
    }
    Orb_NewForce(ORB_FORCE_SHOT, 0);
    shot->moving = false;
    Orb_MoveX(g_Orb->velocity_x);
    Orb_SetTop(Math16_Wrap(g_Orb->top + Orb_UpdateVelocityY()));
    return true;
}

void Shots_UpdateAndRender(const PTN *const sprites)
{
    for (int32_t i = 0; i < SHOTS_COUNT; i++) {
        SHOT *const shot = &m_Shots.slots[i];
        if (shot->moving) {
            M_Restore(shot);
            shot->top = Math16_Wrap(shot->top - 12);
            if (!g_Orb->in_portal && Shots_HitTestOrb(i)) {
                continue;
            }
            if (shot->top >= 64) {
                M_PutShot(sprites, shot);
            } else {
                shot->moving = false;
            }
        } else if (shot->decay_frame != 0) {
            M_Erase(sprites, shot);
            shot->decay_frame++;
            if (shot->decay_frame > 7) {
                shot->decay_frame = 0;
                shot->moving = false;
            } else {
                M_PutShot(sprites, shot);
            }
        }
    }
}

bool Shots_HitTestPellet(const int16_t left, const int16_t top)
{
    for (int32_t i = 0; i < SHOTS_COUNT; i++) {
        SHOT *const shot = &m_Shots.slots[i];
        const int16_t dx = Math16_Wrap(shot->left - left);
        const int16_t dy = Math16_Wrap(shot->top - top);
        if (shot->moving && shot->decay_frame != 1 && dx <= 6 && dx >= -14
            && dy <= 6 && dy >= -14) {
            M_Hit(shot);
            return true;
        }
    }
    return false;
}

bool Shots_HitTestBoss(
    const int16_t left, const int16_t top, const int16_t width_minus_shot,
    const int16_t height_minus_shot)
{
    for (int32_t i = 0; i < SHOTS_COUNT; i++) {
        SHOT *const shot = &m_Shots.slots[i];
        if (shot->moving && shot->left >= left && shot->top >= top
            && shot->left <= Math16_Wrap(left + width_minus_shot)
            && shot->top <= Math16_Wrap(top + height_minus_shot)) {
            M_Hit(shot);
            return true;
        }
    }
    return false;
}
