#include "th01/finite.h"
#include "th01/orb_private.h"
#include "th01/rng.h"
#include "th01/sprite.h"

#include "pc98/zun/master_rng.h"
#include "pc98/zun/math16.h"

#include <string.h>

static ORB m_Orb;
ORB *const p_Orb = &m_Orb;
const ORB *const g_Orb = &m_Orb;

void Orb_Init(void)
{
    memset(&m_Orb, 0, sizeof(m_Orb));
}

static bool M_Left(const ORB_VELOCITY_X velocity)
{
    return velocity == ORB_X_LEFT_4 || velocity == ORB_X_LEFT_8;
}

static bool M_Right(const ORB_VELOCITY_X velocity)
{
    return velocity == ORB_X_RIGHT_4 || velocity == ORB_X_RIGHT_8;
}

void Orb_ResetFlight(void)
{
    m_Orb.left = m_Orb.previous_left = ORB_BOUNDS_MAX_X - 8;
    m_Orb.top = m_Orb.previous_top = ORB_BOUNDS_MAX_Y - 88;
    m_Orb.force = -8.0;
    m_Orb.force_frame = 0;
    m_Orb.velocity_x = ORB_X_LEFT_4;
}

void Orb_AdvanceForceFrame(void)
{
    m_Orb.force_frame = Math16_Wrap(m_Orb.force_frame + 1);
}

int16_t Orb_UpdateVelocityY(void)
{
    const FINITE gravity = Finite_Add(
        Finite_FromDouble(m_Orb.force),
        Finite_FromDouble(m_Orb.force_frame / 5));
    m_Orb.velocity_y = Finite_ToDouble(gravity);
    if (m_Orb.velocity_y > 16.0) {
        m_Orb.velocity_y = 16.0;
    } else if (m_Orb.velocity_y < -16.0) {
        m_Orb.velocity_y = -16.0;
    }
    return Finite_ToPixel(gravity);
}

void Orb_NewForce(const ORB_FORCE mode, const double immediate)
{
    const FINITE velocity = Finite_FromDouble(m_Orb.velocity_y);
    if (mode == ORB_FORCE_SURFACE) {
        m_Orb.force = Finite_ToDouble(
            Finite_Multiply(Finite_Negate(velocity), Finite_FromDouble(0.78)));
        if (m_Orb.velocity_x == ORB_X_STILL && m_Orb.force_frame < 17) {
            if (MasterRng_Next(&g_Rng) % 50 == 0) {
                m_Orb.velocity_x = ORB_X_LEFT_4;
            }
            if (MasterRng_Next(&g_Rng) % 50 == 1) {
                m_Orb.velocity_x = ORB_X_RIGHT_4;
            }
        }
    }
    if (mode == ORB_FORCE_TOP) {
        m_Orb.force = Finite_ToDouble(Finite_Add(
            Finite_Negate(velocity),
            Finite_Negate(Finite_FromDouble(m_Orb.force_frame / 4))));
    }
    if (mode == ORB_FORCE_SHOT) {
        m_Orb.force = Finite_ToDouble(
            Finite_Add(Finite_FromDouble(-10.0), Finite_Half(velocity)));
    }
    if (mode == ORB_FORCE_IMMEDIATE) {
        m_Orb.force = immediate;
    }
    m_Orb.force_frame = 0;
}

void Orb_ReflectX(void)
{
    switch (m_Orb.velocity_x) {
    case ORB_X_LEFT_4:
        m_Orb.velocity_x = ORB_X_RIGHT_4;
        break;
    case ORB_X_RIGHT_4:
        m_Orb.velocity_x = ORB_X_LEFT_4;
        break;
    case ORB_X_LEFT_8:
        m_Orb.velocity_x = ORB_X_RIGHT_8;
        break;
    case ORB_X_RIGHT_8:
        m_Orb.velocity_x = ORB_X_LEFT_8;
        break;
    default:
        m_Orb.velocity_x = ORB_X_STILL;
        break;
    }
}

void Orb_ReflectY(void)
{
    Orb_NewForce(ORB_FORCE_IMMEDIATE, -m_Orb.velocity_y);
}

void Orb_MoveX(const ORB_VELOCITY_X requested)
{
    switch (requested) {
    case ORB_X_LEFT_4:
        m_Orb.left = Math16_Wrap(m_Orb.left - 4);
        break;
    case ORB_X_RIGHT_4:
        m_Orb.left = Math16_Wrap(m_Orb.left + 4);
        break;
    case ORB_X_LEFT_8:
        m_Orb.left = Math16_Wrap(m_Orb.left - 8);
        break;
    case ORB_X_RIGHT_8:
        m_Orb.left = Math16_Wrap(m_Orb.left + 8);
        break;
    default:
        break;
    }
    if (m_Orb.left <= ORB_BOUNDS_MIN_X && M_Left(m_Orb.velocity_x)) {
        Orb_ReflectX();
    }
    if (m_Orb.left >= ORB_BOUNDS_MAX_X && M_Right(m_Orb.velocity_x)) {
        Orb_ReflectX();
    }
}

int16_t Orb_Image(void)
{
    return (int16_t)(m_Orb.rotation_frame / ORB_FRAMES_PER_IMAGE);
}

void Orb_AdvanceMotion(int16_t *const card_combo)
{
    if (m_Orb.in_portal) {
        return;
    }
    Orb_MoveX(m_Orb.velocity_x);
    m_Orb.top = Math16_Wrap(m_Orb.top + Orb_UpdateVelocityY());
    m_Orb.frames_outside_portal = Math16_Wrap(m_Orb.frames_outside_portal + 1);
    if (M_Left(m_Orb.velocity_x)) {
        m_Orb.rotation_frame = Math16_Wrap(m_Orb.rotation_frame + 1);
    }
    if (M_Right(m_Orb.velocity_x)) {
        m_Orb.rotation_frame = Math16_Wrap(m_Orb.rotation_frame - 1);
    }
    if (m_Orb.rotation_frame >= ORB_IMAGE_COUNT * ORB_FRAMES_PER_IMAGE - 2) {
        m_Orb.rotation_frame = 0;
    }
    if (m_Orb.rotation_frame < 0) {
        m_Orb.rotation_frame = (ORB_IMAGE_COUNT - 1) * ORB_FRAMES_PER_IMAGE;
    }
    if (m_Orb.top > ORB_BOUNDS_MAX_Y) {
        Orb_NewForce(ORB_FORCE_SURFACE, 0.78);
        m_Orb.top = ORB_BOUNDS_MAX_Y;
        *card_combo = 0;
    }
    if (m_Orb.top < ORB_BOUNDS_MIN_Y) {
        Orb_NewForce(ORB_FORCE_TOP, 0.0);
        m_Orb.top = ORB_BOUNDS_MIN_Y;
    }
}

bool Orb_PlayerHitTest(
    const ORB_PLAYER *const player, const int16_t repel_friction)
{
    const int32_t delta_x = player->left - m_Orb.left;
    const int32_t delta_y = player->top - m_Orb.top;
    const int32_t abs_x = delta_x < 0 ? -delta_x : delta_x;
    const int32_t abs_y = delta_y < 0 ? -delta_y : delta_y;
    if (repel_friction == ORB_REPEL_NONE) {
        return !player->invincible && !player->invincible_against_orb
            && abs_x < 24 && abs_y < 16;
    }
    if (abs_x >= 40 || abs_y >= 32) {
        return false;
    }
    if (repel_friction < ORB_REPEL_3_X_UNCHANGED) {
        if (delta_x > 0) {
            m_Orb.velocity_x = ORB_X_LEFT_4;
        } else if (delta_x == 0) {
            m_Orb.velocity_x = ORB_X_STILL;
            if (MasterRng_Next(&g_Rng) % 8 == 0) {
                m_Orb.velocity_x = ORB_X_LEFT_4;
            }
            if (MasterRng_Next(&g_Rng) % 8 == 4) {
                m_Orb.velocity_x = ORB_X_LEFT_4;
            }
        } else {
            m_Orb.velocity_x = ORB_X_RIGHT_4;
        }
        Orb_NewForce(ORB_FORCE_IMMEDIATE, repel_friction / 2 - 13);
    } else {
        switch (repel_friction) {
        case ORB_REPEL_3_X_LEFT_4:
            m_Orb.velocity_x = ORB_X_LEFT_4;
            break;
        case ORB_REPEL_3_X_RIGHT_4:
            m_Orb.velocity_x = ORB_X_RIGHT_4;
            break;
        case ORB_REPEL_3_X_RIGHT_8:
            m_Orb.velocity_x = ORB_X_RIGHT_8;
            break;
        case ORB_REPEL_3_X_LEFT_8:
            m_Orb.velocity_x = ORB_X_LEFT_8;
            break;
        default:
            break;
        }
        Orb_NewForce(ORB_FORCE_IMMEDIATE, -10.0);
    }
    return false;
}

void Orb_Put(
    const PTN *const sprites, const int16_t left, const int16_t top,
    const int16_t image)
{
    Sprite_PtnPut(sprites, ORB_FIRST_IMAGE + image, -1, left, top);
}

void Orb_Erase(
    const PTN *const sprites, const int16_t left, const int16_t top,
    const int16_t image)
{
    Sprite_PtnErase(sprites, ORB_FIRST_IMAGE + image, -1, left, top);
}

void Orb_Relaunch(void)
{
    m_Orb.left = ORB_BOUNDS_MAX_X - 8;
    m_Orb.top = ORB_BOUNDS_MAX_Y - 88;
    m_Orb.force = -8.0;
    m_Orb.force_frame = 0;
    m_Orb.velocity_x = ORB_X_LEFT_4;
}

void Orb_LeavePortal(void)
{
    m_Orb.in_portal = false;
}

void Orb_Teleport(const int16_t left, const int16_t top)
{
    m_Orb.left = left;
    m_Orb.top = top;
    Orb_LeavePortal();
}

void Orb_EnterPortal(void)
{
    m_Orb.in_portal = true;
}

void Orb_SetTop(const int16_t top)
{
    m_Orb.top = top;
}

void Orb_SetVelocityX(const ORB_VELOCITY_X velocity)
{
    m_Orb.velocity_x = velocity;
}

void Orb_KeepPosition(void)
{
    m_Orb.previous_left = m_Orb.left;
    m_Orb.previous_top = m_Orb.top;
}
